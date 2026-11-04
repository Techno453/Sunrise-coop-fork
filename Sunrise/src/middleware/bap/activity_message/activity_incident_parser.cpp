/** Validates incident framing and rejects targets unsafe for the Client's unbounded table read. */

#include <algorithm>

#include "../../encoding/bit_reader.h"
#include "../../encoding/bit_writer.h"
#include "../../encoding/byte_order.h"
#include "incident.h"
#include "kill_incident_codec.h"

namespace sunrise::middleware::bap::activity_message::incident {
namespace {

/** Trailing padding completes the last body byte. */
constexpr std::size_t kMaximumPaddingBits = 7;
/** Widest bit run copied in one step. */
constexpr std::size_t kCopyChunkBits = 64;

/** Retains byte fields even when their wire start is not byte-aligned. */
[[nodiscard]] bool read_bytes(encoding::bits::Reader& reader,
                              std::span<std::byte> output) noexcept {
    for (std::byte& value : output) {
        std::uint64_t field = 0;
        if (!reader.read(encoding::kBitsPerByte, field)) {
            return false;
        }
        value = static_cast<std::byte>(field);
    }
    return true;
}

/** @return True when one target index is safe to hand to the Client's table lookup. */
[[nodiscard]] bool target_allowed(std::uint32_t target, Verdict& verdict) noexcept {
    if (target > kTargetMaximum) {
        verdict = Verdict::targetOutOfRange;
        return false;
    }
    if (std::find(kPoisonTargets.begin(), kPoisonTargets.end(), target) != kPoisonTargets.end()) {
        verdict = Verdict::targetPoisoned;
        return false;
    }
    return true;
}

} // namespace

/** @return A short stable name for one verdict, for the log line. */
const char* verdict_name(Verdict verdict) noexcept {
    switch (verdict) {
    case Verdict::accepted:
        return "accepted";
    case Verdict::truncated:
        return "truncated";
    case Verdict::targetOutOfRange:
        return "target_out_of_range";
    case Verdict::targetPoisoned:
        return "target_poisoned";
    case Verdict::tooManyTargets:
        return "too_many_targets";
    case Verdict::payloadTooLong:
        return "payload_too_long";
    case Verdict::selectorTooLong:
        return "selector_too_long";
    }
    return "unknown";
}

/** Validates one incident body from its first target to the end of its payload. */
Verdict validate(std::span<const std::byte> payload, Incident& parsed) noexcept {
    parsed = {};
    encoding::bits::Reader reader(payload);

    std::uint64_t field = 0;
    if (!reader.read(kTargetWidth, field)) {
        return Verdict::truncated;
    }
    parsed.primaryTarget = static_cast<std::uint32_t>(field);
    Verdict verdict = Verdict::accepted;
    if (!target_allowed(parsed.primaryTarget, verdict)) {
        return verdict;
    }

    if (!reader.read(kExtraCountWidth, field)) {
        return Verdict::truncated;
    }
    parsed.extraTargetCount = static_cast<std::uint32_t>(field);
    if (parsed.extraTargetCount > kExtraTargetMaximum) {
        return Verdict::tooManyTargets;
    }
    for (std::uint32_t index = 0; index < parsed.extraTargetCount; ++index) {
        if (!reader.read(kTargetWidth, field)) {
            return Verdict::truncated;
        }
        parsed.extraTargets[index] = static_cast<std::uint32_t>(field);
        if (!target_allowed(parsed.extraTargets[index], verdict)) {
            return verdict;
        }
    }

    if (!reader.read(kSelectorPresenceWidth, field)) {
        return Verdict::truncated;
    }
    parsed.hasCompressedSelector = field != 0;
    if (parsed.hasCompressedSelector) {
        if (!reader.read(kSelectorLengthWidth, field)) {
            return Verdict::truncated;
        }
        parsed.selectorLength = static_cast<std::uint32_t>(field);
        if (parsed.selectorLength > kSelectorMaximum) {
            return Verdict::selectorTooLong;
        }
        if (!read_bytes(reader, std::span(parsed.selector).first(parsed.selectorLength))) {
            return Verdict::truncated;
        }
    }

    if (!reader.read(kOptionalPresenceWidth, field)) {
        return Verdict::truncated;
    }
    parsed.hasOptionalBlock = field != 0;
    if (parsed.hasOptionalBlock) {
        if (!reader.read(kOptionalWordWidth, field)) {
            return Verdict::truncated;
        }
        parsed.optionalWordA = static_cast<std::uint32_t>(field);
        if (!reader.read(kOptionalWordWidth, field)) {
            return Verdict::truncated;
        }
        parsed.optionalWordB = static_cast<std::uint32_t>(field);
    }

    if (!reader.read(kPayloadLengthWidth, field)) {
        return Verdict::truncated;
    }
    parsed.payloadLength = static_cast<std::uint32_t>(field);
    if (parsed.payloadLength > kPayloadMaximum) {
        return Verdict::payloadTooLong;
    }
    if (!read_bytes(reader, std::span(parsed.payload).first(parsed.payloadLength))) {
        return Verdict::truncated;
    }
    parsed.consumedBits = static_cast<std::uint32_t>(payload.size() * encoding::kBitsPerByte
                                                     - reader.remaining_bits());
    return Verdict::accepted;
}

bool fully_framed(std::span<const std::byte> payload, Incident& parsed) noexcept {
    if (validate(payload, parsed) != Verdict::accepted || parsed.payloadLength == 0
        || parsed.consumedBits > payload.size() * encoding::kBitsPerByte) {
        return false;
    }
    const std::size_t padding = payload.size() * encoding::kBitsPerByte - parsed.consumedBits;
    if (padding > kMaximumPaddingBits) {
        return false;
    }
    encoding::bits::Reader reader(payload);
    std::uint64_t value = 0;
    return reader.skip(parsed.consumedBits)
           && reader.read(static_cast<std::uint8_t>(padding), value) && value == 0
           && reader.remaining_bits() == 0;
}

bool extract_body(std::span<const std::byte> input,
                  const Incident& parsed,
                  std::span<std::byte> output) noexcept {
    const std::size_t payloadBits = parsed.payloadLength * encoding::kBitsPerByte;
    if (parsed.payloadLength > output.size() || parsed.consumedBits < payloadBits) {
        return false;
    }
    encoding::bits::Reader reader(input);
    if (!reader.skip(parsed.consumedBits - payloadBits)) {
        return false;
    }
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < parsed.payloadLength; ++i) {
        if (!reader.read(encoding::kBitsPerByte, value)) {
            return false;
        }
        output[i] = static_cast<std::byte>(value);
    }
    return true;
}

bool relayable(std::span<const std::byte> payload, Incident& parsed) noexcept {
    if (!fully_framed(payload, parsed)) {
        return false;
    }
    if (parsed.primaryTarget != kCompactKillTarget) {
        return !parsed.hasCompressedSelector;
    }
    if (parsed.extraTargetCount != 0) {
        return false;
    }
    std::array<std::byte, kPayloadMaximum> body{};
    return extract_body(payload, parsed, body)
           && canonical_kill_payload(std::span(body).first(parsed.payloadLength));
}

bool prepare_peer_relay(std::span<const std::byte> payload,
                        std::span<std::byte> output,
                        std::size_t& written,
                        std::uint32_t& sourceSchema) noexcept {
    written = 0;
    sourceSchema = 0;
    Incident parsed{};
    if (!fully_framed(payload, parsed)) {
        return false;
    }
    if (parsed.primaryTarget != kCompactKillTarget && !parsed.hasCompressedSelector) {
        if (payload.size() > output.size()) {
            return false;
        }
        std::copy(payload.begin(), payload.end(), output.begin());
        written = payload.size();
        return true;
    }
    if (parsed.primaryTarget != kCompactKillTarget || parsed.extraTargetCount != 0) {
        return false;
    }
    std::array<std::byte, kPayloadMaximum> body{}, role2{}, role3{};
    if (!extract_body(payload, parsed, body)) {
        return false;
    }
    const auto input = std::span(body).first(parsed.payloadLength);
    std::size_t bytes2 = 0, bytes3 = 0, bits2 = 0, bits3 = 0;
    const bool valid2 =
        transcode_kill_payload(input, KillSourceRole::compact2, role2, bytes2, bits2);
    const bool valid3 =
        transcode_kill_payload(input, KillSourceRole::compact3, role3, bytes3, bits3);
    if (valid2 == valid3) {
        return false;
    }
    const auto canonical = valid2 ? std::span(role2).first(bytes2) : std::span(role3).first(bytes3);
    if (!canonical_kill_payload(canonical)) {
        return false;
    }
    std::array<std::byte, kMaximumBodyBytes> staged{};
    encoding::bits::Writer writer(staged);
    encoding::bits::Reader reader(payload);
    std::size_t prefix =
        parsed.consumedBits - (parsed.payloadLength * encoding::kBitsPerByte) - kPayloadLengthWidth;
    while (prefix != 0) {
        const auto width = static_cast<std::uint8_t>(std::min(kCopyChunkBits, prefix));
        std::uint64_t value = 0;
        if (!reader.read(width, value) || !writer.write(value, width)) {
            return false;
        }
        prefix -= width;
    }
    if (!writer.write(canonical.size(), kPayloadLengthWidth)) {
        return false;
    }
    for (std::byte value : canonical) {
        if (!writer.write(std::to_integer<std::uint8_t>(value), encoding::kBitsPerByte)) {
            return false;
        }
    }
    std::size_t bytes = 0;
    if (!writer.finish(bytes) || bytes > output.size()) {
        return false;
    }
    Incident checked{};
    if (!relayable(std::span(staged).first(bytes), checked)) {
        return false;
    }
    std::copy_n(staged.begin(), bytes, output.begin());
    written = bytes;
    sourceSchema = valid2 ? kCompact2Schema : kCompact3Schema;
    return true;
}

} // namespace sunrise::middleware::bap::activity_message::incident
