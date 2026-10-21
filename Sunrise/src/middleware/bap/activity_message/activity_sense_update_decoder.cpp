#include "../../encoding/bit_reader.h"
#include "../../encoding/byte_order.h"
#include "sense_roster_delta.h"
#include "sense_update.h"
#include "sense_update_body.h"

namespace sunrise::middleware::bap::activity_message::sense_update {
namespace {
namespace bits = middleware::encoding::bits;

/** Field widths of the object header ClientRef, in bits. */
constexpr std::uint8_t kKeyWidth = 32, kTypeWidth = 7, kIndexWidth = 16;
/** The slot type and index are sent unsigned, biased so their absent value is zero. */
constexpr std::uint32_t kTypeBias = 1, kIndexBias = 32768;

using body::decode_body;
using body::NativeStatus;
using body::Reader;
using body::Values;

[[nodiscard]] bool skip_group(Reader& reader) noexcept {
    if (reader.left() == 0 || !reader.skip(reader.left() - 1)) {
        return false;
    }
    std::uint64_t end = 0;
    return reader.read(1, end) && end == 0 && reader.left() == 0;
}
void finish(SenseUpdate& update,
            bits::Reader& reader,
            std::size_t total,
            DecodeStatus status,
            std::size_t& consumed) noexcept {
    update.decoded.status = status;
    update.decoded.bitsConsumed = total - reader.remaining_bits();
    update.decoded.bitsRemaining = reader.remaining_bits();
    consumed = update.decoded.bitsConsumed;
}
} // namespace

/** Decodes epoch and sensor blocks with per-group budgets and exact zero padding. */
bool decode_sense_update(std::span<const std::byte> input,
                         const Resolver& resolver,
                         SenseUpdate& update,
                         std::size_t& consumed) noexcept {
    update = {};
    consumed = 0;
    if (input.size() > kOuterByteCapacity) {
        return false;
    }
    const std::size_t total = input.size() * encoding::kBitsPerByte;
    bits::Reader reader(input);
    std::uint64_t literal = 0, root = 0;
    if (!reader.read(kEpochFieldWidth, update.epoch.first)
        || !reader.read(kEpochFieldWidth, update.epoch.second) || !reader.read(1, literal)
        || literal != 0 || !reader.read(1, root)) {
        finish(update, reader, total, DecodeStatus::malformed, consumed);
        return false;
    }
    update.tailBits = static_cast<std::uint32_t>(reader.remaining_bits() + 1);
    // The groups follow the mirrored roster delta, so a set root bit means skip its body first.
    if (root != 0) {
        const std::size_t before = reader.remaining_bits();
        if (!skip_roster_delta(reader)) {
            finish(update, reader, total, DecodeStatus::schemaUnavailable, consumed);
            return true;
        }
        update.rosterDeltaBits = static_cast<std::uint32_t>(before - reader.remaining_bits());
    }
    bool partial = false;
    for (;;) {
        std::uint64_t groupPresent = 0;
        if (!reader.read(1, groupPresent)) {
            finish(update, reader, total, DecodeStatus::malformed, consumed);
            return false;
        }
        if (groupPresent == 0) {
            break;
        }
        std::uint64_t groupKey = 0, groupBits = 0;
        if (!reader.read(32, groupKey) || !reader.read(32, groupBits)
            || groupBits > kGroupByteCapacity * 8 || groupBits > reader.remaining_bits()) {
            finish(update, reader, total, DecodeStatus::malformed, consumed);
            return false;
        }
        ++update.decoded.groupsSeen;
        GroupTarget group{};
        const TargetStatus groupStatus =
            resolver.resolveGroup == nullptr
                ? TargetStatus::targetUnavailable
                : resolver.resolveGroup(
                      resolver.context, static_cast<std::uint32_t>(groupKey), group);
        Reader body(reader, static_cast<std::size_t>(groupBits), total);
        if (groupStatus != TargetStatus::resolved) {
            if (!skip_group(body)) {
                finish(update, reader, total, DecodeStatus::malformed, consumed);
                return false;
            }
            ++update.decoded.groupsSkipped;
            partial = true;
            continue;
        }
        bool skipped = false, hasObject = false;
        while (!skipped) {
            std::uint64_t objectPresent = 0;
            if (!body.read(1, objectPresent)) {
                finish(update, reader, total, DecodeStatus::malformed, consumed);
                return false;
            }
            if (objectPresent == 0) {
                if (!hasObject || body.left() != 0) {
                    finish(update, reader, total, DecodeStatus::malformed, consumed);
                    return false;
                }
                ++update.decoded.groupsDecoded;
                break;
            }
            hasObject = true;
            ++update.decoded.objectsSeen;
            std::uint64_t objectKey = 0, typeRaw = 0, indexRaw = 0;
            if (!body.read(kKeyWidth, objectKey) || objectKey != groupKey
                || !body.read(kTypeWidth, typeRaw) || typeRaw < kTypeBias
                || !body.read(kIndexWidth, indexRaw) || indexRaw < kIndexBias) {
                finish(update, reader, total, DecodeStatus::malformed, consumed);
                return false;
            }
            const auto type = static_cast<std::uint8_t>(typeRaw - kTypeBias);
            const auto index = static_cast<std::uint16_t>(indexRaw - kIndexBias);
            if (!update.hasFirstObject) {
                update.firstGroupBits = static_cast<std::uint32_t>(groupBits);
                update.firstRegistryKey = static_cast<std::uint32_t>(groupKey);
                update.firstSlotType = type;
                update.firstSlotIndex = index;
                update.hasFirstObject = true;
            }
            DecodedObject* retained = nullptr;
            if (update.decoded.objectCount < update.decoded.objects.size()) {
                retained = &update.decoded.objects[update.decoded.objectCount++];
                retained->registryKey = static_cast<std::uint32_t>(groupKey);
                retained->objectTag = group.objectTag;
                retained->objectRow = group.objectRow;
                retained->slotType = type;
                retained->slotIndex = index;
                retained->firstValue = static_cast<std::uint32_t>(update.decoded.valueCount);
            } else {
                update.decoded.objectsTruncated = true;
            }
            SlotTarget slot{};
            const TargetStatus slotStatus =
                resolver.resolveSlot == nullptr
                    ? TargetStatus::targetUnavailable
                    : resolver.resolveSlot(resolver.context, group, type, index, slot);
            if (retained != nullptr) {
                retained->slotRow = slot.slotRow;
                retained->senseSchema = slot.senseSchema;
                retained->schemaRow = slot.senseSchema;
            }
            if (slotStatus != TargetStatus::resolved) {
                if (retained != nullptr) {
                    retained->status = slotStatus == TargetStatus::schemaUnavailable
                                           ? ObjectStatus::schemaUnavailable
                                           : ObjectStatus::targetUnavailable;
                }
                if (!skip_group(body)) {
                    finish(update, reader, total, DecodeStatus::malformed, consumed);
                    return false;
                }
                skipped = true;
                partial = true;
                ++update.decoded.groupsSkipped;
                continue;
            }
            const std::size_t start = body.position();
            Values values(update.decoded, retained);
            const NativeStatus status = decode_body(slot.senseSchema, body, values);
            if (retained != nullptr) {
                retained->deltaBits = static_cast<std::uint32_t>(body.position() - start);
                retained->valueCount =
                    static_cast<std::uint32_t>(update.decoded.valueCount - retained->firstValue);
                retained->status = status == NativeStatus::complete      ? ObjectStatus::decoded
                                   : status == NativeStatus::unsafeCount ? ObjectStatus::unsafeCount
                                   : status == NativeStatus::unsupported
                                       ? ObjectStatus::unsupportedField
                                       : ObjectStatus::malformed;
            }
            if (status == NativeStatus::malformed) {
                finish(update, reader, total, DecodeStatus::malformed, consumed);
                return false;
            }
            if (status != NativeStatus::complete) {
                if (!skip_group(body)) {
                    finish(update, reader, total, DecodeStatus::malformed, consumed);
                    return false;
                }
                skipped = true;
                partial = true;
                ++update.decoded.groupsSkipped;
                continue;
            }
            std::uint64_t generation = 0;
            if (!body.read(32, generation)) {
                finish(update, reader, total, DecodeStatus::malformed, consumed);
                return false;
            }
            if (retained != nullptr) {
                retained->generationPlusOne = static_cast<std::uint32_t>(generation);
                retained->hasGeneration = true;
                retained->status = ObjectStatus::decoded;
            }
            ++update.decoded.objectsDecoded;
        }
    }
    std::uint64_t tail = 0;
    if (!reader.read(1, tail) || tail != 0 || reader.remaining_bits() > 7) {
        finish(update, reader, total, DecodeStatus::malformed, consumed);
        return false;
    }
    update.decoded.paddingBits = static_cast<std::uint8_t>(reader.remaining_bits());
    std::uint64_t padding = 0;
    if (!reader.read(update.decoded.paddingBits, padding) || padding != 0) {
        finish(update, reader, total, DecodeStatus::malformed, consumed);
        return false;
    }
    finish(
        update, reader, total, partial ? DecodeStatus::partial : DecodeStatus::complete, consumed);
    return true;
}

/** @return Stable log spelling for one packet decode status. */
const char* decode_status_name(DecodeStatus status) noexcept {
    switch (status) {
    case DecodeStatus::complete:
        return "complete";
    case DecodeStatus::partial:
        return "partial";
    case DecodeStatus::schemaUnavailable:
        return "unsupported_native_layout";
    case DecodeStatus::malformed:
        return "malformed";
    }
    return "unknown";
}
/** @return Stable log spelling for one object decode status. */
const char* object_status_name(ObjectStatus status) noexcept {
    switch (status) {
    case ObjectStatus::decoded:
        return "decoded";
    case ObjectStatus::targetUnavailable:
        return "target_unavailable";
    case ObjectStatus::schemaUnavailable:
    case ObjectStatus::unsupportedField:
        return "unsupported_native_layout";
    case ObjectStatus::unsafeCount:
        return "unsafe_count";
    case ObjectStatus::malformed:
        return "malformed";
    }
    return "unknown";
}
} // namespace sunrise::middleware::bap::activity_message::sense_update
