#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "../../content/packages/tables/incident_targets.h"

namespace sunrise::middleware::bap::activity_message::incident {

/**
 * Kill reason (canonical `+0x35C`) the native ordinary-victim classifier F8FC70 refuses; native
 * 790C2A95 never classifies it as an opposing kill.
 */
inline constexpr std::uint8_t kExcludedKillReason = 2;

/**
 * Native kill report facts. Every faction field here holds the team the native faction names:
 * a team faction is read as its native partition (`auth_fields::faction_team`).
 */
struct KillFacts final {
    std::uint32_t sequence{};
    std::uint64_t sourcePlayer{};
    std::uint64_t killingPlayer{};
    std::uint64_t deadPlayer{};
    std::int8_t killerFaction{-1};
    std::int8_t victimFaction{-1};
    std::uint8_t reason{};
    bool ordinaryVictim{};
};

/** Native cinematic playback report, including the authored sensor reference. */
struct CinematicFacts final {
    std::uint32_t sequence{};
    std::uint64_t sourcePlayer{};
    std::uint32_t group{};
    std::uint8_t type{};
    std::uint16_t slot{};
    /** Native 106A530 obtains the component clock and converts it with 35FC00. */
    std::uint64_t timestampMilliseconds{};
    float duration{};
};

/** Native capture participant capacity: 0x503160 registers the C808087ba array with sixteen. */
inline constexpr std::size_t kCaptureParticipantCapacity = 16;

/** Faction the capture component (0xED5070) posts when it neutralizes a zone; no team. */
inline constexpr std::int8_t kNeutralFaction = -2;

/**
 * Native type-6 zone report (`C80806470`), posted by the capture component of the peer that
 * simulates the zone: 0xED5070 when its progress completes and 0xED7AD0 when an ownership change
 * arrives with no occupants. Its factions are read as teams, as in `KillFacts`.
 */
struct CaptureFacts final {
    std::uint32_t sequence{};
    std::uint64_t sourcePlayer{};
    /** Previous owner, component `+0x31`. */
    std::int8_t losingFaction{-1};
    /** New owner, or kNeutralFaction for a neutralized zone. */
    std::int8_t capturingFaction{-1};
    /** Native handle of the zone object, `+0x328`. */
    std::uint32_t zone{};
    /** Occupant keys of the progressing faction; their native key bytes become scalar SOIDs. */
    std::array<std::uint64_t, kCaptureParticipantCapacity> players{};
    std::array<std::int8_t, kCaptureParticipantCapacity> factions{};
    std::uint8_t count{};
};

/** Reads one complete type-6 payload. */
[[nodiscard]] bool read_capture_facts(std::span<const std::byte> input,
                                      CaptureFacts& output) noexcept;

/** Reads one complete type-17 playback report from a player source. */
[[nodiscard]] bool read_cinematic_facts(std::span<const std::byte> input,
                                        CinematicFacts& output) noexcept;

/** Native type-25 amount event, authored by an activity authority with no local player. */
[[nodiscard]] bool
encode_score_notice(const content::packages::tables::incident_targets::Catalog& catalog,
                    std::uint16_t target,
                    std::uint32_t sequence,
                    std::uint64_t player,
                    std::int32_t amount,
                    std::span<std::byte> output,
                    std::size_t& written) noexcept;

/** Reads a canonical type-1 kill; participant object keys become host scalar SOIDs. */
[[nodiscard]] bool read_kill_facts(std::span<const std::byte> input, KillFacts& output) noexcept;

/** Native type-1 serializer roles selected by 0xF91370. */
enum class KillSourceRole : std::uint8_t { compact2 = 2, compact3 = 3 };
/** The compact kill schema each serializer role writes. */
inline constexpr std::uint32_t kCompact2Schema =
    content::packages::schema::generated::C80806447::kClass;
inline constexpr std::uint32_t kCompact3Schema =
    content::packages::schema::generated::C80806442::kClass;

/**
 * Expands a complete compact type-1 payload to the receiver's C8080645a wire schema.
 * Ports 0xF901B0 / 0xF902F0; preserves account and character keys. This does not select recipients
 * or produce scores. Unsupported/truncated bodies and nonzero padding fail without output.
 */
[[nodiscard]] bool transcode_kill_payload(std::span<const std::byte> input,
                                          KillSourceRole role,
                                          std::span<std::byte> output,
                                          std::size_t& written,
                                          std::size_t& meaningfulBits) noexcept;

/** Accepts only the canonical subset transcode_kill_payload produces, with absent actor handles. */
[[nodiscard]] bool canonical_kill_payload(std::span<const std::byte> input) noexcept;

} // namespace sunrise::middleware::bap::activity_message::incident
