#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../content/packages/schema/generated/tag_classes_80804d.h"
#include "../../encoding/bit_writer.h"
#include "auth_fields.h"
#include "match_object_auth.h"
#include "object_marker_auth.h"

// The capture component's authority block for one capture zone, carried in the zone object's
// type-4 Auth. The zone's component reads it in 0xED7820.

namespace sunrise::middleware::bap::activity_message::capture_zone {

namespace fields = auth_fields;

/** Capture component authority block, the `C80809aec` entry 0xED7820 looks up by class. */
inline constexpr std::uint32_t kComponentState =
    content::packages::schema::generated::C80804df7::kClass;
/** Capture component reply 0xED6960 writes into the object Sense under the same message. */
inline constexpr std::uint32_t kComponentReply =
    content::packages::schema::generated::C80804df6::kClass;
/** The reply's optional progress is a four-bit quantized unit fraction. */
inline constexpr std::uint8_t kReplyProgressWidth = 4;
/** Native faction of a zone nobody holds, as the capture component writes it. */
inline constexpr std::int8_t kNeutral = -2;
/** The optional duration (`C80802da5`) and optional byte (`C80802da3`) arrays: one bit each. */
inline constexpr std::uint8_t kOptionalCountWidth = 1;
/**
 * Capture block: the zone index, the contested revision and flag, the owner revision and owner, a
 * ClientRef and two absent options.
 */
inline constexpr std::size_t kComponentBits =
    fields::kSigned32Width + fields::kSigned32Width + fields::kBoolWidth + fields::kSigned32Width
    + fields::kFactionWidth + fields::kClientRefBits + 2 * kOptionalCountWidth;

/** Authority state of one zone. Zero revisions leave the simulating peer's own state untouched. */
struct State final {
    /**
     * Contested revision (component `+0x68`); a newer one applies `contested` (`+0x61`). While
     * `+0x61` is set the zone is locked: 0xED8160 clears its progressing faction, so nobody
     * captures it. A definition whose `+0x60` bit 0 is set latches it on a capture (0xED5070).
     */
    std::int32_t contestedRevision{};
    bool contested{};
    /** Owner revision (component `+0x64`); a newer one hands the zone to `owner` (0xED7AD0). */
    std::int32_t ownerRevision{};
    std::int8_t owner{kNeutral};
    /**
     * The zone's index among the mode's zones in authored order. 0xED7820 copies it into component
     * `+0x6C` on every apply to a zone this peer simulates (bit 0x26BE0E0), with no revision gate;
     * the score-strip HUD (0xBEAF30, built while the lifetime names it) letters its zone icons from
     * `+0x6C`, which 0xED4500 returns as the first field of the zone's record. The floating banner
     * has a separate index in object_marker::kComponentState.
     */
    std::int32_t index{};
};

/**
 * Writes the `kComponentState` body that follows its class key.
 * @return False for a negative revision or zone index, or an unencodable owner.
 */
[[nodiscard]] inline bool write_component(encoding::bits::Writer& writer,
                                          const State& state) noexcept {
    if (state.contestedRevision < 0 || state.ownerRevision < 0 || state.index < 0
        || state.owner < fields::kMinimumFaction || state.owner > fields::kMaximumFaction) {
        return false;
    }
    const auto start = writer.bit_count();
    const std::array<fields::Field, 5> component{{
        {static_cast<std::uint32_t>(state.index) + fields::kSigned32Bias, fields::kSigned32Width},
        {static_cast<std::uint32_t>(state.contestedRevision) + fields::kSigned32Bias,
         fields::kSigned32Width},
        {state.contested ? 1U : 0U, fields::kBoolWidth},
        {static_cast<std::uint32_t>(state.ownerRevision) + fields::kSigned32Bias,
         fields::kSigned32Width},
        {static_cast<std::uint32_t>(state.owner + fields::kFactionBias), fields::kFactionWidth},
    }};
    return fields::write_fields(writer, component) && fields::write_absent_client_ref(writer)
           && writer.write(0, kOptionalCountWidth) && writer.write(0, kOptionalCountWidth)
           && writer.bit_count() - start == kComponentBits;
}

/**
 * Builds one zone's match object, optionally naming its separate world marker too. Control's
 * capture component drives the score-strip letters; its stateful marker draws the floating banner.
 * @return False for a nonpositive generation or a state `write_component` refuses; `output` is
 * then unchanged.
 */
[[nodiscard]] inline bool object(match_object::Object& output,
                                 std::uint32_t group,
                                 std::uint16_t slot,
                                 std::int32_t generation,
                                 const State& state,
                                 bool letteredMarker = false) noexcept {
    match_object::Object zone{group, slot, generation};
    zone.componentCount = 1;
    if (generation <= 0
        || !match_object::pack(
            zone.components[0], kComponentState, [&state](encoding::bits::Writer& writer) noexcept {
                return write_component(writer, state);
            })) {
        return false;
    }
    if (letteredMarker) {
        if (!match_object::pack(zone.components[1],
                                object_marker::kComponentState,
                                [&state](encoding::bits::Writer& writer) noexcept {
                                    return object_marker::write_index(writer, state.index);
                                })) {
            return false;
        }
        zone.componentCount = 2;
    }
    output = zone;
    return true;
}

} // namespace sunrise::middleware::bap::activity_message::capture_zone
