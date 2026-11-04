#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../../content/packages/schema/generated/tag_classes_80804f.h"
#include "../../encoding/bit_writer.h"
#include "auth_fields.h"

namespace sunrise::middleware::bap::activity_message::object_marker {

namespace fields = auth_fields;

/** Stateful world-marker Auth; 0x10036E0 applies it on the object's simulating peer. */
inline constexpr std::uint32_t kComponentState =
    content::packages::schema::generated::C80804f8a::kClass;
/** Native unset hash: retain the marker definition's state and text selections. */
inline constexpr std::uint32_t kAuthored = 0x811C9DC5U;
/** Five present optional fields, a ClientRef, visibility, and four 32-bit values. */
inline constexpr std::size_t kComponentBits = 5 * fields::kPresenceWidth + fields::kClientRefBits
                                              + fields::kBoolWidth + 4 * fields::kSigned32Width;

/**
 * Names a visible marker by index, keeping its authored state and text. Schema 0x3816E80:
 * ClientRef, visible, state hash, signed index, primary text hash, secondary text hash.
 * 0x10036E0 copies the index to component +0x270; 0xFF0E80 uses it for the glyph frame.
 * @return False for a negative index or insufficient writer space.
 */
[[nodiscard]] inline bool write_index(encoding::bits::Writer& writer, std::int32_t index) noexcept {
    if (index < 0) {
        return false;
    }
    const auto start = writer.bit_count();
    const std::array<fields::Field, 9> marker{{
        {1, fields::kBoolWidth}, // visible
        {1, fields::kPresenceWidth},
        {kAuthored, fields::kSigned32Width}, // authored state
        {1, fields::kPresenceWidth},
        {static_cast<std::uint32_t>(index) + fields::kSigned32Bias, fields::kSigned32Width},
        {1, fields::kPresenceWidth},
        {kAuthored, fields::kSigned32Width}, // authored primary text
        {1, fields::kPresenceWidth},
        {kAuthored, fields::kSigned32Width}, // authored secondary text
    }};
    return writer.write(1, fields::kPresenceWidth) && fields::write_absent_client_ref(writer)
           && fields::write_fields(writer, marker) && writer.bit_count() - start == kComponentBits;
}

} // namespace sunrise::middleware::bap::activity_message::object_marker
