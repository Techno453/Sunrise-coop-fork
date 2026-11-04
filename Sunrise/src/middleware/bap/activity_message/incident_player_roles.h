#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sunrise::middleware::bap::activity_message::incident {

/** Players one role binds. */
inline constexpr std::size_t kRolePlayerCapacity = 10;
/** Roles one interface binds; a kill binds the most. */
inline constexpr std::size_t kRoleCapacity = 4;

/** Native role keys the payload interfaces bind, by role name. */
inline constexpr std::uint32_t kKillingPlayerRole = 0x4FE19919U;
inline constexpr std::uint32_t kDeadPlayerRole = 0x8703A279U;
inline constexpr std::uint32_t kPlayerRole = 0x3FBE3C2AU;
inline constexpr std::uint32_t kResurrectingPlayerRole = 0xC7161F86U;
/**
 * Name unknown. Native callback 0xA63C50 fills it with the filtered assist account keys; the
 * authority binds every player participant of the kill to it.
 */
inline constexpr std::uint32_t kAssistRole = 0x9555E15AU;
/** Name unknown. The kill's fourth role: the killing player, then each player participant. */
inline constexpr std::uint32_t kDamageParticipantRole = 0x80B51494U;
/** `participant_player`, which 0xF8EBD0 binds to each participant's key. */
inline constexpr std::uint32_t kParticipantPlayerRole = 0x4CF2C008U;

/** One role key and the players bound to it, in native order. */
struct PlayerRole final {
    std::uint32_t key{};
    std::array<std::uint64_t, kRolePlayerCapacity> players{};
    std::uint8_t count{};
};
/** The roles one incident binds, with its source player and sequence. */
struct PlayerRoles final {
    std::uint64_t sourcePlayer{};
    std::uint32_t sequence{};
    std::array<PlayerRole, kRoleCapacity> roles{};
    std::uint8_t count{};
};

/** Canonical type-1/3/6/13/20/40 player interfaces; preserve native role order and duplicates. */
[[nodiscard]] bool
read_player_roles(std::span<const std::byte> input, std::int8_t type, PlayerRoles& output) noexcept;

} // namespace sunrise::middleware::bap::activity_message::incident
