#include <limits>

#include "../../content/packages/schema/generated/tag_classes_80807e.h"
#include "../../encoding/bit_reader.h"
#include "../../encoding/byte_order.h"
#include "auth_fields.h"
#include "capture_zone_auth.h"
#include "interactable_object_auth.h"
#include "match_object_auth.h"
#include "sensor_auth_update.h"
#include "sensor_sense_update.h"

namespace sunrise::middleware::bap::activity_message::sensor_sense_update {
namespace {

namespace auth = sensor_auth_update;
namespace fields = auth_fields;
namespace generated = content::packages::schema::generated;

/** Field widths the walked schemas share. */
constexpr std::uint8_t kFlagWidth = fields::kPresenceWidth;
/** A u32, an i32, a raw float or a hash. */
constexpr std::uint8_t kWordWidth = 32;
/** A 64-bit key. */
constexpr std::uint8_t kLongWidth = 64;
/** An optional 31-bit signed scalar. */
constexpr std::uint8_t kScalarWidth = 31;
/** A six-bit signed scalar. */
constexpr std::uint8_t kShortScalarWidth = 6;
/** A seven-bit quantized real. */
constexpr std::uint8_t kQuantizedWidth = 7;
/** A nine-bit quantized spatial real. */
constexpr std::uint8_t kSpatialWidth = 9;

/** An opaque prelude precedes the roster delta. */
constexpr std::size_t kPreludeBits = 128;
/**
 * The smallest body: the prelude, then a zero flag, an absent roster delta, the empty group list's
 * terminator and the closing zero flag.
 */
constexpr std::size_t kMinimumBodyBits = kPreludeBits + 4;
constexpr std::size_t kMaximumPaddingBits = 7;
/** Bounds the objects one message may walk, so a hostile body cannot run the walker long. */
constexpr std::size_t kMaximumObjects = 4'096;
/** Every object closes with one trailer word. */
constexpr std::uint8_t kObjectTrailerWidth = kWordWidth;
/** Slot types whose Sense this walker reads. */
constexpr std::uint8_t kSquadSlotType = 1;
constexpr std::uint8_t kMemberSlotType = 2;
constexpr std::uint8_t kObjectSlotType = match_object::kSlotType;
constexpr std::uint8_t kParticipationSlotType = 13;
/** Type-4 object Sense (C8080992e) after its generation: two flags, the entry index, two masks. */
constexpr std::size_t kObjectHeadBits = kFlagWidth + kFlagWidth + kWordWidth + (2 * kWordWidth);
constexpr std::uint8_t kObjectReplyCountWidth = match_object::kComponentCountWidth;
/** Registered object replies: interaction latch, ownership key, zone capture. */
constexpr std::uint32_t kInteractionReply = interactable_object::kInteractionReply;
constexpr std::uint32_t kOwnershipReply = interactable_object::kOwnershipReply;
constexpr std::uint32_t kCaptureReply = capture_zone::kComponentReply;
constexpr std::uint32_t kFactionReply = generated::C80807e63::kClass;
constexpr std::size_t kOwnershipReplyBits = kFlagWidth + kLongWidth;
/**
 * Capture reply (0xED6960): owner and progressing factions (six bits each), contested flag and
 * two revisions, then an optional player occupancy mask and optional four-bit progress.
 */
constexpr std::uint8_t kCaptureProgressBits = capture_zone::kReplyProgressWidth;

/** Squad Sense C80807ecc: six optional root scalars, a 2-bit and a 3-bit enum, three booleans. */
constexpr std::array<std::uint8_t, 6> kSquadScalarWidths = {
    kScalarWidth, kScalarWidth, kScalarWidth, kShortScalarWidth, kQuantizedWidth, kScalarWidth};
constexpr std::size_t kSquadRequiredBits = 2 + 3 + (3 * kFlagWidth);
/** Optional squad list: a four-bit count of at most eight words, then that many words. */
constexpr std::uint8_t kSquadCountWidth = 4;
constexpr std::uint64_t kSquadCountCapacity = 8;
/** C80807ecd: a fixed array of 24 optional quantized reals. */
constexpr std::size_t kSquadRealCount = 24;
/** Member Sense C80807da2: f7's fixed array C80807da4 holds eight optional scalars. */
constexpr std::size_t kMemberLaneCount = 8;
/** The member's required 2-bit state. */
constexpr std::size_t kMemberStateBits = 2;
/** Participation C808094e4: the ready bits, region, loading and partition, high bit first. */
constexpr std::uint8_t kReadyWidth = 3;
constexpr std::uint64_t kRegionReadyBit = 4;
constexpr std::uint64_t kLoadingCompleteBit = 2;
constexpr std::uint64_t kPartitionReadyBit = 1;
/** The fourth boolean, then the 3-bit and 2-bit enums. */
constexpr std::size_t kParticipationEnumBits = kFlagWidth + 3 + 2;
/** C80804f35: a three-bit count of hash and raw-float pairs. */
constexpr std::uint8_t kValueCountWidth = 3;
/** C808094dd: a five-bit class and a six-bit faction, then two counted arrays of at most 32. */
constexpr std::size_t kIdentityBits = 5 + fields::kFactionWidth;
constexpr std::uint8_t kIdentityCountWidth = 6;
constexpr std::uint64_t kIdentityCapacity = 32;
/** C80804f31 opens with a boolean and a four-bit field. */
constexpr std::size_t kParticipationFlagBits = kFlagWidth + 4;
/** Two optional 16-bit fields follow. */
constexpr std::uint8_t kHalfWidth = 16;
/** The root closes with a boolean, a byte and the signed i32 tail. */
constexpr std::size_t kParticipationTailBits = kFlagWidth + 8 + kWordWidth;
/** The roster delta's counted state bytes. */
constexpr std::size_t kRosterStateWidth = 8;

/** Adds an explicit bit limit and position to the shared MSB-first reader. */
class Cursor final {
public:
    explicit Cursor(std::span<const std::byte> input) noexcept
        : reader_(input), capacity_(input.size() * encoding::kBitsPerByte), limit_(capacity_) {}

    [[nodiscard]] bool read(std::uint8_t width, std::uint64_t& value) noexcept {
        if (position_ > limit_ || width > limit_ - position_ || !reader_.read(width, value)) {
            return false;
        }
        position_ += width;
        return true;
    }

    [[nodiscard]] bool skip(std::size_t width) noexcept {
        if (position_ > limit_ || width > limit_ - position_ || !reader_.skip(width)) {
            return false;
        }
        position_ += width;
        return true;
    }

    [[nodiscard]] std::size_t position() const noexcept {
        return position_;
    }
    [[nodiscard]] std::size_t limit() const noexcept {
        return limit_;
    }
    [[nodiscard]] std::size_t capacity() const noexcept {
        return capacity_;
    }

    [[nodiscard]] bool set_limit(std::size_t limit) noexcept {
        if (limit < position_ || limit > capacity_) {
            return false;
        }
        limit_ = limit;
        return true;
    }

private:
    encoding::bits::Reader reader_;
    std::size_t capacity_{};
    std::size_t limit_{};
    std::size_t position_{};
};

[[nodiscard]] bool read_zero(Cursor& cursor, std::uint8_t width) noexcept {
    std::uint64_t value = 0;
    return cursor.read(width, value) && value == 0;
}

/** Reads one opcode-0x100 optional primitive. */
[[nodiscard]] bool
read_optional(Cursor& cursor, std::uint8_t width, bool& present, std::uint64_t& value) noexcept {
    std::uint64_t flag = 0;
    value = 0;
    if (!cursor.read(kFlagWidth, flag)) {
        return false;
    }
    present = flag != 0;
    return !present || cursor.read(width, value);
}

[[nodiscard]] bool skip_optional(Cursor& cursor, std::uint8_t width) noexcept {
    bool present = false;
    std::uint64_t value = 0;
    return read_optional(cursor, width, present, value);
}

/** Walks squad sense C80807ecc without assigning semantics to any non-overlapping field. */
[[nodiscard]] bool parse_squad(Cursor& cursor) noexcept {
    for (const std::uint8_t width : kSquadScalarWidths) {
        if (!skip_optional(cursor, width)) {
            return false;
        }
    }
    if (!cursor.skip(kSquadRequiredBits)) {
        return false;
    }

    std::uint64_t present = 0;
    if (!cursor.read(kFlagWidth, present)) {
        return false;
    }
    if (present != 0) {
        std::uint64_t count = 0;
        if (!cursor.read(kSquadCountWidth, count) || count > kSquadCountCapacity
            || !cursor.skip(static_cast<std::size_t>(count) * kWordWidth)) {
            return false;
        }
    }

    if (!cursor.read(kFlagWidth, present)) {
        return false;
    }
    if (present != 0) {
        for (std::size_t index = 0; index < kSquadRealCount; ++index) {
            if (!skip_optional(cursor, kQuantizedWidth)) {
                return false;
            }
        }
    }
    return true;
}

/** Walks optional member-sense branch f3 (C80807f6e). */
[[nodiscard]] bool parse_member_f3(Cursor& cursor) noexcept {
    return skip_optional(cursor, kShortScalarWidth) && skip_optional(cursor, kScalarWidth)
           && skip_optional(cursor, kScalarWidth) && cursor.skip(kFlagWidth);
}

/** Walks optional member-sense branch f7 (C80807da3). */
[[nodiscard]] bool parse_member_f7(Cursor& cursor) noexcept {
    std::uint64_t present = 0;
    if (!cursor.read(kFlagWidth, present)) {
        return false;
    }
    if (present != 0) {
        for (std::size_t index = 0; index < kMemberLaneCount; ++index) {
            if (!skip_optional(cursor, kScalarWidth)) {
                return false;
            }
        }
    }
    return skip_optional(cursor, kWordWidth);
}

/** Walks member sense C80807da2. */
[[nodiscard]] bool parse_member(Cursor& cursor) noexcept {
    if (!skip_optional(cursor, kScalarWidth) || !skip_optional(cursor, kSpatialWidth)
        || !skip_optional(cursor, kScalarWidth)) {
        return false;
    }

    std::uint64_t present = 0;
    if (!cursor.read(kFlagWidth, present) || (present != 0 && !parse_member_f3(cursor))) {
        return false;
    }
    if (!cursor.read(kFlagWidth, present) || (present != 0 && !parse_member_f7(cursor))) {
        return false;
    }

    // f18, inherited required 2-bit state, f19, f20, f21, and two required booleans.
    return skip_optional(cursor, kScalarWidth) && cursor.skip(kMemberStateBits)
           && skip_optional(cursor, kScalarWidth) && skip_optional(cursor, kQuantizedWidth)
           && skip_optional(cursor, kQuantizedWidth) && cursor.skip(2 * kFlagWidth);
}

/**
 * Walks native participation sense C80804f2f. Counted arrays use their preceding wire count,
 * bounded by the native capacity.
 */
[[nodiscard]] bool parse_participation(Cursor& cursor,
                                       ParticipationObservation& observation) noexcept {
    // C808094e4: two optional i/u32, four booleans, then 3/2-bit enums.
    std::uint64_t ready{};
    if (!skip_optional(cursor, kWordWidth) || !skip_optional(cursor, kWordWidth)
        || !cursor.read(kReadyWidth, ready) || !cursor.skip(kParticipationEnumBits)) {
        return false;
    }
    observation.regionReady = (ready & kRegionReadyBit) != 0;
    observation.loadingComplete = (ready & kLoadingCompleteBit) != 0;
    observation.partitionReady = (ready & kPartitionReadyBit) != 0;
    // C80804f35 -> C80804f38/C80804f37/C80804f39: count + at most four hash/raw-float pairs.
    std::uint64_t count = 0;
    if (!cursor.read(kValueCountWidth, count) || count > observation.values.size()) {
        return false;
    }
    observation.valueCount = static_cast<std::size_t>(count);
    for (std::size_t index = 0; index < observation.valueCount; ++index) {
        std::uint64_t name = 0;
        std::uint64_t value = 0;
        if (!cursor.read(kWordWidth, name) || !cursor.read(kWordWidth, value)) {
            return false;
        }
        observation.values[index] = {static_cast<std::uint32_t>(name),
                                     static_cast<std::uint32_t>(value)};
    }
    // Separate raw float, partition enum, optional i32, three booleans.
    std::uint64_t partition{};
    if (!cursor.skip(kWordWidth) || !cursor.read(auth::kPartitionWidth, partition)
        || !skip_optional(cursor, kWordWidth) || !cursor.skip(3 * kFlagWidth)) {
        return false;
    }
    observation.partition =
        static_cast<std::int8_t>(static_cast<int>(partition) - auth::kPartitionBias);
    // C808094dd: optional 64-bit object key, class/faction, two counted arrays.
    if (!skip_optional(cursor, kLongWidth) || !cursor.skip(kIdentityBits)
        || !cursor.read(kIdentityCountWidth, count) || count > kIdentityCapacity
        || !cursor.skip(static_cast<std::size_t>(count) * kWordWidth)
        || !cursor.read(kIdentityCountWidth, count) || count > kIdentityCapacity) {
        return false;
    }
    for (std::uint64_t index = 0; index < count; ++index) {
        if (!skip_optional(cursor, kWordWidth) || !skip_optional(cursor, kLongWidth)) {
            return false;
        }
    }
    // C80804f31 and required signed-i32 root tail.
    return cursor.skip(kParticipationFlagBits) && skip_optional(cursor, kHalfWidth)
           && skip_optional(cursor, kHalfWidth) && skip_optional(cursor, kWordWidth)
           && cursor.skip(kParticipationTailBits);
}

/** Skips the optional roster delta. */
[[nodiscard]] bool parse_roster_delta(Cursor& cursor) noexcept {
    std::uint64_t present = 0;
    if (!cursor.read(kFlagWidth, present)) {
        return false;
    }
    if (present == 0) {
        return true;
    }
    if (!cursor.read(kFlagWidth, present)) {
        return false;
    }
    if (present == 0) {
        return true;
    }

    std::uint64_t count = 0;
    if (!cursor.read(kFlagWidth, present)) {
        return false;
    }
    if (present != 0
        && (!cursor.read(auth::kDeltaCountWidth, count)
            || count > (std::numeric_limits<std::size_t>::max)() / auth::kKeyWidth
            || !cursor.skip(static_cast<std::size_t>(count) * auth::kKeyWidth))) {
        return false;
    }
    if (!cursor.read(kFlagWidth, present)
        || (present != 0 && !cursor.skip(auth::kDeltaMaskWords * kWordWidth))) {
        return false;
    }
    if (!cursor.read(kFlagWidth, present)) {
        return false;
    }
    if (present != 0
        && (!cursor.read(auth::kDeltaCountWidth, count)
            || count > (std::numeric_limits<std::size_t>::max)() / kRosterStateWidth
            || !cursor.skip(static_cast<std::size_t>(count) * kRosterStateWidth))) {
        return false;
    }
    return read_zero(cursor, kFlagWidth);
}

/** Walks known type-4 reply schemas so later participation stays readable. */
[[nodiscard]] bool skip_object(Cursor& cursor) noexcept {
    std::uint64_t count{};
    if (!cursor.skip(kWordWidth + kObjectHeadBits) || !cursor.read(kObjectReplyCountWidth, count)) {
        return false;
    }
    for (std::uint64_t index = 0; index < count; ++index) {
        std::uint64_t exists{}, schema{};
        if (!cursor.read(kFlagWidth, exists)) {
            return false;
        }
        if (exists == 0) {
            continue;
        }
        if (!cursor.read(match_object::kClassKeyWidth, schema)) {
            return false;
        }
        if (schema == kInteractionReply) {
            if (!cursor.skip(kFlagWidth + kWordWidth)) {
                return false;
            }
        } else if (schema == kCaptureReply) {
            if (!cursor.skip(2 * fields::kFactionWidth + kFlagWidth + 2 * kWordWidth)
                || !skip_optional(cursor, kWordWidth)
                || !skip_optional(cursor, kCaptureProgressBits)) {
                return false;
            }
        } else if (schema == kFactionReply) {
            if (!cursor.skip(fields::kFactionWidth)) {
                return false;
            }
        } else if (schema != kOwnershipReply || !cursor.skip(kOwnershipReplyBits)) {
            return false;
        }
    }
    return true;
}

/**
 * Parses one bounded group content region and restores the outer limit. A body this parser cannot
 * read (an unknown slot type or reply class, or a full observation table) has no length of its
 * own, so the rest of the group is skipped to its bound; what the group yielded before it stays.
 * @return False only when the group's own bound cannot be read; the message is then unreadable.
 */
[[nodiscard]] bool parse_group(Cursor& cursor, Update& update, std::size_t& objectCount) noexcept {
    std::uint64_t containerKey = 0;
    std::uint64_t contentBits = 0;
    if (!cursor.read(auth::kKeyWidth, containerKey) || !cursor.read(auth::kKeyWidth, contentBits)
        || contentBits > cursor.limit() - cursor.position()) {
        return false;
    }
    const std::size_t outerLimit = cursor.limit();
    const std::size_t groupEnd = cursor.position() + static_cast<std::size_t>(contentBits);
    if (!cursor.set_limit(groupEnd)) {
        return false;
    }

    bool valid = true;
    bool terminated = false;
    while (valid && cursor.position() < groupEnd) {
        std::uint64_t present = 0;
        if (!cursor.read(kFlagWidth, present)) {
            valid = false;
            break;
        }
        if (present == 0) {
            terminated = true;
            break;
        }
        if (++objectCount > kMaximumObjects) {
            valid = false;
            break;
        }

        std::uint64_t objectKey = 0;
        std::uint64_t slotTypeWire = 0;
        std::uint64_t slotIndexWire = 0;
        if (!cursor.read(auth::kKeyWidth, objectKey)
            || !cursor.read(auth::kSlotTypeWidth, slotTypeWire)
            || !cursor.read(auth::kSlotIndexWidth, slotIndexWire)
            || slotTypeWire < auth::kSlotTypeBias || slotIndexWire < auth::kSlotIndexBias
            || objectKey != containerKey) {
            valid = false;
            break;
        }
        const auto slotType = static_cast<std::uint8_t>(slotTypeWire - auth::kSlotTypeBias);
        const auto slotIndex = static_cast<std::uint16_t>(slotIndexWire - auth::kSlotIndexBias);
        if (!cursor.read(kFlagWidth, present)) {
            valid = false;
            break;
        }
        if (present != 0) {
            if (slotType == kSquadSlotType) {
                valid = parse_squad(cursor);
            } else if (slotType == kMemberSlotType) {
                valid = parse_member(cursor);
            } else if (slotType == kParticipationSlotType) {
                if (update.participationCount >= update.participation.size()) {
                    valid = false;
                } else {
                    auto& observation = update.participation[update.participationCount++];
                    observation.key = static_cast<std::uint32_t>(objectKey);
                    observation.slotIndex = slotIndex;
                    valid = parse_participation(cursor, observation);
                    if (!valid) {
                        // A participation body read only in part says nothing reliable.
                        observation = {};
                        --update.participationCount;
                    }
                }
            } else if (slotType == kObjectSlotType) {
                valid = skip_object(cursor);
            } else {
                // There is no per-object body length, so an unknown present schema cannot be
                // skipped without guessing where its u32 trailer begins.
                valid = false;
            }
        }
        if (valid) {
            valid = cursor.skip(kObjectTrailerWidth);
        }
    }

    // An unreadable body ends this group's walk; its bound still frames the next group.
    if (!(valid && terminated && cursor.position() == groupEnd)
        && !cursor.skip(groupEnd - cursor.position())) {
        return false;
    }
    return cursor.set_limit(outerLimit);
}

} // namespace

bool parse(std::span<const std::byte> input, Update& update) noexcept {
    update = {};
    if (input.size() > (std::numeric_limits<std::size_t>::max)() / encoding::kBitsPerByte
        || input.size() * encoding::kBitsPerByte < kMinimumBodyBits) {
        return false;
    }

    Cursor cursor(input);
    Update parsed{};
    if (!cursor.skip(kPreludeBits) || !read_zero(cursor, kFlagWidth)
        || !parse_roster_delta(cursor)) {
        return false;
    }

    std::size_t objectCount = 0;
    for (;;) {
        std::uint64_t present = 0;
        if (!cursor.read(kFlagWidth, present)) {
            return false;
        }
        if (present == 0) {
            break;
        }
        if (!parse_group(cursor, parsed, objectCount)) {
            return false;
        }
    }
    if (!read_zero(cursor, kFlagWidth)) {
        return false;
    }
    const std::size_t padding = cursor.capacity() - cursor.position();
    if (padding > kMaximumPaddingBits || !cursor.set_limit(cursor.capacity())
        || !read_zero(cursor, static_cast<std::uint8_t>(padding))
        || cursor.position() != cursor.capacity()) {
        return false;
    }
    update = parsed;
    return true;
}

} // namespace sunrise::middleware::bap::activity_message::sensor_sense_update
