#pragma once

#include <cstddef>
#include <cstdint>

#include "../../encoding/bit_writer.h"
#include "auth_fields.h"

namespace sunrise::middleware::bap::activity_message::cinematic_record {

namespace fields = auth_fields;

/** Slot type of the cinematic command block. */
inline constexpr std::uint8_t kSlotType = 6;
/** Cinematic commands one match can hold. */
inline constexpr std::size_t kCommandCapacity = 8;
/** The command opens with two unset 64-bit handles, then its 32-bit generation. */
inline constexpr std::uint8_t kHandleWidth = 64;
inline constexpr std::uint8_t kGenerationWidth = 32;
/** The six-bit role carries the native role plus three; wire 2 is the any-role -1. */
inline constexpr std::uint8_t kRoleWidth = 6;
inline constexpr std::uint32_t kAnyRoleWire = 2;
/** Counts of the empty player list (C808094f8, at most 16) and slot list (C80804f0b, at most 6). */
inline constexpr std::uint8_t kPlayerCountWidth = 5;
inline constexpr std::uint8_t kSlotCountWidth = 3;
/** The body closes with one zero word. */
inline constexpr std::uint8_t kTailWidth = 32;
inline constexpr std::size_t kBodyBits =
    (2 * kHandleWidth) + kGenerationWidth + (2 * fields::kBoolWidth) + fields::kClientRefBits
    + kRoleWidth + kPlayerCountWidth + kSlotCountWidth + kTailWidth;

/** One cinematic command: its template group, slot, generation and whether it plays. */
struct Command final {
    std::uint32_t group{};
    std::uint32_t generation{};
    std::uint16_t slot{};
    bool active{};
};

/** Writes the C80804f08 command body native 10697B0 consumes. @return False for no generation. */
[[nodiscard]] inline bool write_body(encoding::bits::Writer& writer,
                                     const Command& command) noexcept {
    if (command.generation == 0) {
        return false;
    }
    const auto start = writer.bit_count();
    return writer.write(0, kHandleWidth) && writer.write(0, kHandleWidth)
           && writer.write(command.generation, kGenerationWidth)
           && writer.write(command.active ? 1U : 0U, fields::kBoolWidth)
           && writer.write(0, fields::kBoolWidth) && fields::write_absent_client_ref(writer)
           && writer.write(kAnyRoleWire, kRoleWidth) && writer.write(0, kPlayerCountWidth)
           && writer.write(0, kSlotCountWidth) && writer.write(0, kTailWidth)
           && writer.bit_count() - start == kBodyBits;
}

} // namespace sunrise::middleware::bap::activity_message::cinematic_record
