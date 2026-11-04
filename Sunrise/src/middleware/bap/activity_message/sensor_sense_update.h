#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sunrise::middleware::bap::activity_message::sensor_sense_update {

/** Activity message type 6 carries client-sensed component deltas. */
inline constexpr std::uint32_t kMessageType = 6;
/** Participation observations one message may contribute. */
inline constexpr std::size_t kParticipationCapacity = 64;

/** Native C80804f39 named value, retained without assigning score semantics. */
struct ParticipationValue final {
    std::uint32_t nameHash{};
    std::uint32_t floatBits{};
};

/** Named values one participation carries in C80804f35. */
inline constexpr std::size_t kParticipationValueCapacity = 4;

/** Client observations from C80804f2f. */
struct ParticipationObservation final {
    std::uint32_t key{};
    std::uint16_t slotIndex{};
    std::array<ParticipationValue, kParticipationValueCapacity> values{};
    std::size_t valueCount{};
    std::int8_t partition{-1};
    bool regionReady{};
    bool loadingComplete{};
    bool partitionReady{};
};

/** Fields extracted from every group of one msg-6 body the parser can read. */
struct Update final {
    std::array<ParticipationObservation, kParticipationCapacity> participation{};
    std::size_t participationCount{};
};

/**
 * Parses the msg-6 framing, complete squad/member/participation sense schemas and the
 * registered replies of type-4 objects without retaining unused object telemetry. A body a group
 * cannot be read past (an unknown slot schema or object reply, a nonzero unknown branch, a full
 * table) ends that group's walk at its bound and keeps what it yielded before; a bad group bound,
 * outer framing or nonzero padding rejects the whole update.
 */
[[nodiscard]] bool parse(std::span<const std::byte> input, Update& update) noexcept;

} // namespace sunrise::middleware::bap::activity_message::sensor_sense_update
