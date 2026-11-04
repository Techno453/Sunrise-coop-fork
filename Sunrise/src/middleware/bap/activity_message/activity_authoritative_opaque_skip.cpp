#include <bit>

#include "../../gameplay/group/native_player_profile.h"
#include "client_authoritative_data.h"

namespace sunrise::middleware::bap::activity_message::client_authoritative_data {
namespace {

/** The B2 byte-list count uses 14 bits and allows at most 10,000 elements. */
constexpr std::uint8_t kLargeArrayCountWidth = 14;
/** The B2 byte-list schema holds 10,000 elements. */
constexpr std::uint64_t kLargeArrayMaximumCount = 10'000;
/** The D9 byte-list count uses 8 bits and allows at most 128 elements. */
constexpr std::uint8_t kSmallArrayCountWidth = 8;
/** The D9 byte-list schema holds 128 elements. */
constexpr std::uint64_t kSmallArrayMaximumCount = 128;
/** Every dynamic element in both opaque branches is one byte. */
constexpr std::size_t kArrayElementBitCount = 8;
/** A presence bit. */
constexpr std::uint8_t kPresenceWidth = 1;
/** The nested player blob (C21820): a 3-bit kind and a 1-bit owned index ahead of its A block. */
constexpr std::uint8_t kPlayerBlobHeaderWidth = 3 + 1;
/** 16D4080's optional name: 16-bit units ending in a zero unit, within the native capacity. */
constexpr std::uint8_t kNameUnitWidth = 16;
constexpr std::size_t kNameUnitCapacity = gameplay::group::kNativePlayerNameCapacity;

/**
 * Skips one optional fixed-width field.
 * @param reader Reader sitting at the field marker.
 * @param width Field width when the field is present.
 * @return True when the marker and any present value fit.
 */
[[nodiscard]] bool skip_optional(encoding::bits::Reader& reader, std::size_t width) noexcept {
    bool present = false;
    return read_presence(reader, present) && (!present || reader.skip(width));
}

/**
 * Skips one D9 record, including its dynamic byte list.
 * @param reader Reader sitting at the first required D9 field.
 * @return True when the count is valid and the whole record fits.
 */
[[nodiscard]] bool skip_d9(encoding::bits::Reader& reader) noexcept {
    std::uint64_t count = 0;
    return reader.skip(8) && reader.skip(2) && reader.read(kSmallArrayCountWidth, count)
           && count <= kSmallArrayMaximumCount
           && reader.skip(static_cast<std::size_t>(count) * kArrayElementBitCount)
           && reader.skip(64);
}

/** The leg's first scalar is a 10-bit slice-set index at bias 1. */
constexpr std::uint8_t kSliceSetIndexWidth = 10;
/** The leg's second scalar is that slice set's 32-bit name hash. */
constexpr std::uint8_t kSliceSetHashWidth = 32;
/** The leg's third scalar is the 32-bit region index consumed by the region table. */
constexpr std::uint8_t kRegionIndexWidth = 32;
/** The signed region index uses the unsigned midpoint as its wire bias. */
constexpr std::uint32_t kRegionIndexBias = 0x80000000U;
/** Fields 3 and 4 are 2-bit signed values at bias 1. */
constexpr std::uint8_t kLegStateWidth = 2;
constexpr std::int32_t kLegStateBias = 1;

/**
 * Reads one complete D6 leg. Both legs have the same shape. The first scalar names a slice set;
 * the third names the region matched by the membership region table. The optional aux token and
 * the session sub-block are walked and dropped.
 * @param reader Reader sitting at the first required D6 scalar.
 * @param region Receives the leg's scalars.
 * @return True when every required scalar and optional child fits.
 */
[[nodiscard]] bool read_d6(encoding::bits::Reader& reader, RegionState& region) noexcept {
    std::uint64_t sliceSetIndexWire = 0;
    std::uint64_t sliceSetHash = 0;
    std::uint64_t regionIndexWire = 0;
    std::uint64_t publicWire = 0;
    std::uint64_t auxWire = 0;
    if (!reader.read(kSliceSetIndexWidth, sliceSetIndexWire)
        || !reader.read(kSliceSetHashWidth, sliceSetHash)
        || !reader.read(kRegionIndexWidth, regionIndexWire)
        || !reader.read(kLegStateWidth, publicWire) || !reader.read(kLegStateWidth, auxWire)
        || !skip_optional(reader, 8)) {
        return false;
    }
    const auto stored = static_cast<std::uint32_t>(regionIndexWire) - kRegionIndexBias;
    region.index = std::bit_cast<std::int32_t>(stored);
    region.hash = static_cast<std::uint32_t>(sliceSetHash);
    region.sliceSetIndex = static_cast<std::int32_t>(sliceSetIndexWire) - kLegStateBias;
    region.publicState =
        static_cast<std::int8_t>(static_cast<std::int32_t>(publicWire) - kLegStateBias);
    region.auxState = static_cast<std::int8_t>(static_cast<std::int32_t>(auxWire) - kLegStateBias);
    region.hasHash = true;
    bool d9Present = false;
    return read_presence(reader, d9Present) && (!d9Present || skip_d9(reader));
}

/**
 * Reads the assignment the nested player blob (C21820) at the head of B4 reports.
 * @param nested Reader at the first element of the list.
 * @param endRemaining Bits left in `nested` once the list is consumed; no read passes it.
 * @param update Receives the assigned partition only when the whole prefix reads.
 * @return False when the prefix does not fit the list or its name is unterminated.
 */
[[nodiscard]] bool read_player_assignment(encoding::bits::Reader& nested,
                                          std::size_t endRemaining,
                                          ClientAuthoritativeData& update) noexcept {
    const auto read = [&](std::uint8_t width, std::uint64_t& value) noexcept {
        return nested.remaining_bits() >= endRemaining + width && nested.read(width, value);
    };
    std::uint64_t value = 0;
    if (!read(kPlayerBlobHeaderWidth, value) || !read(kPresenceWidth, value)) {
        return false;
    }
    if (value != 0) {
        bool terminated = false;
        for (std::size_t unit = 0; unit < kNameUnitCapacity; ++unit) {
            if (!read(kNameUnitWidth, value)) {
                return false;
            }
            if (value == 0) {
                terminated = true;
                break;
            }
        }
        if (!terminated) {
            return false;
        }
    }
    if (!read(kPresenceWidth, value)) {
        return false;
    }
    if (value != 0) {
        if (!read(kPlayerAssignmentWidth, value)) {
            return false;
        }
        update.assignedPartition =
            static_cast<std::int8_t>(static_cast<int>(value) - kPlayerAssignmentBias);
        update.hasAssignedPartition = true;
    }
    return true;
}

/**
 * Walks the optional dynamic byte-list field inside B2 and keeps the native assignment the
 * nested player blob reports. A list whose prefix does not read as a player blob carries no
 * assignment; the report around it still stands.
 * @param reader Reader sitting at the B4 presence marker.
 * @param update Receives the assigned partition when the block carries one.
 * @return True when the marker is absent, or the whole list is there and within its limit.
 */
[[nodiscard]] bool read_player_array(encoding::bits::Reader& reader,
                                     ClientAuthoritativeData& update) noexcept {
    bool present = false;
    std::uint64_t count = 0;
    if (!read_presence(reader, present)) {
        return false;
    }
    if (!present) {
        return true;
    }
    if (!reader.read(kLargeArrayCountWidth, count) || count > kLargeArrayMaximumCount) {
        return false;
    }
    // Keep the outer schema walk byte-exact; the nested prefix never reads into its siblings.
    encoding::bits::Reader nested = reader;
    const std::size_t bits = static_cast<std::size_t>(count) * kArrayElementBitCount;
    if (!reader.skip(bits)) {
        return false;
    }
    if (bits != 0) {
        static_cast<void>(read_player_assignment(nested, nested.remaining_bits() - bits, update));
    }
    return true;
}

} // namespace

bool read_presence(encoding::bits::Reader& reader, bool& present) noexcept {
    present = false;
    std::uint64_t wire = 0;
    if (!reader.read(1, wire)) {
        return false;
    }
    present = wire != 0;
    return true;
}

/** Keeps B2 fields 0, 10 and 11 from this exact sparse report. */
bool read_transport_branch(encoding::bits::Reader& reader,
                           TransportReport& report,
                           ClientAuthoritativeData* update) noexcept {
    ClientAuthoritativeData ignored{};
    report = {};
    TransportReport parsed{};
    std::uint64_t flags{};
    const auto address = [&reader](auto& bytes, bool& present) noexcept {
        if (!read_presence(reader, present)) {
            return false;
        }
        if (!present) {
            return true;
        }
        for (auto& byte : bytes) {
            std::uint64_t value{};
            if (!reader.read(8, value)) {
                return false;
            }
            byte = static_cast<std::byte>(value);
        }
        return true;
    };
    // Each skip below is one B2 field at the width its schema declares, in declaration order.
    // The `* 8` widths are byte-array fields; the bare ones are scalars.
    constexpr std::size_t kAddressBits = gameplay::descriptor::kNetAddrSize * 8U;
    const bool walked =
        read_presence(reader, parsed.hasFlags) && (!parsed.hasFlags || reader.read(6, flags))
        && skip_optional(reader, 128U * 8U) && skip_optional(reader, 64)
        && skip_optional(reader, 64) && skip_optional(reader, 64) && skip_optional(reader, 64)
        && skip_optional(reader, 32) && skip_optional(reader, 32) && skip_optional(reader, 8U * 8U)
        && skip_optional(reader, 32) && address(parsed.alternate, parsed.hasAlternate)
        && address(parsed.address, parsed.hasAddress) && skip_optional(reader, kAddressBits)
        && skip_optional(reader, 64U * 8U)
        && read_player_array(reader, update == nullptr ? ignored : *update) && reader.skip(1);
    if (!walked) {
        return false;
    }
    parsed.flags = static_cast<std::uint8_t>(flags);
    report = parsed;
    return true;
}

/** Reads the D4 branch and keeps its optional transition token and both region legs. */
bool read_transition_branch(encoding::bits::Reader& reader,
                            ClientAuthoritativeData& update) noexcept {
    // The first leg is the current region, the second the pending one.
    bool currentPresent = false;
    if (!read_presence(reader, currentPresent)
        || (currentPresent && !read_d6(reader, update.currentRegion))) {
        return false;
    }
    update.hasCurrentRegion = currentPresent;
    bool pendingPresent = false;
    if (!read_presence(reader, pendingPresent)
        || (pendingPresent && !read_d6(reader, update.region))) {
        return false;
    }
    update.hasRegion = pendingPresent;

    bool tokenPresent = false;
    std::uint64_t token = 0;
    if (!read_presence(reader, tokenPresent) || (tokenPresent && !reader.read(8, token))) {
        return false;
    }
    if (tokenPresent) {
        update.transitionToken = static_cast<std::uint8_t>(token);
        update.hasTransitionToken = true;
    }
    return skip_optional(reader, 8) && skip_optional(reader, 3);
}

} // namespace sunrise::middleware::bap::activity_message::client_authoritative_data
