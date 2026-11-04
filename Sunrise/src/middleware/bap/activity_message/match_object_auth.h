#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "../../content/packages/schema/generated/tag_classes_808099.h"
#include "../../encoding/bit_writer.h"
#include "auth_fields.h"

// Type-4 object Auth (`C8080992f`) for the capture zones a match mode spawns. The object entry and
// transform are fixed; each object carries up to three component authority blocks in its
// `C80809aea` list, which the object's components look up by class key.

namespace sunrise::middleware::bap::activity_message::match_object {

namespace fields = auth_fields;

/** Object Auth schema of every type-4 slot. */
inline constexpr std::uint32_t kSchema = content::packages::schema::generated::C8080992f::kClass;
/** Object slot type. */
inline constexpr std::uint8_t kSlotType = 4;
/** Objects one match can spawn: Control's three capture zones. */
inline constexpr std::size_t kCapacity = 3;
/** Width of the `C80809aea` component count. */
inline constexpr std::uint8_t kComponentCountWidth = 2;
/** Component blocks the two-bit count of the `C80809aea` list can declare. */
inline constexpr std::size_t kComponentCapacity = 3;
/** Widest component body a match object carries, in bytes. */
inline constexpr std::size_t kComponentByteCapacity = 32;
/** Packed component bodies fill whole bytes, most significant bit first. */
inline constexpr std::size_t kBitsPerByte = 8;
/** Class key ahead of each `C80809aec` component body. */
inline constexpr std::uint8_t kClassKeyWidth = 32;
/** The transform: three 32-bit words, then a flag. */
inline constexpr std::size_t kTransformWords = 3;
inline constexpr std::uint8_t kTransformWordWidth = 32;
/** Object entry and transform fields ahead of the component list. */
inline constexpr std::size_t kObjectBits =
    fields::kSigned32Width + fields::kSigned32Width + fields::kBoolWidth + fields::kBoolWidth
    + fields::kSigned32Width + fields::kClientRefBits + (kTransformWords * kTransformWordWidth)
    + fields::kBoolWidth;

/** One component authority block: its class key and its MSB-first packed body. */
struct Component final {
    std::uint32_t classKey{};
    std::array<std::byte, kComponentByteCapacity> body{};
    std::uint16_t bitCount{};
};

/** One spawned match object: its template group, type-4 slot, generation and components. */
struct Object final {
    std::uint32_t group{};
    std::uint16_t slot{};
    /** Positive object generation; a new one respawns the object entry. */
    std::int32_t generation{};
    /**
     * Entry field .2. A cleared one spawns nothing, and the type-4 Auth apply (0x9F19F0)
     * destroys the entity it finds live (0x56A8F0).
     */
    bool active{true};
    std::array<Component, kComponentCapacity> components{};
    std::uint8_t componentCount{};
};

/**
 * Packs one component body. `write` receives a writer over the component's bytes and writes the
 * body that follows the class key.
 * @return False when `write` fails or the body does not fit; `output` is then unchanged.
 */
template <typename Write>
[[nodiscard]] bool pack(Component& output, std::uint32_t classKey, Write&& write) noexcept {
    Component packed{};
    packed.classKey = classKey;
    encoding::bits::Writer writer{std::span<std::byte>(packed.body)};
    std::size_t bytes{};
    if (!write(writer) || !writer.finish(bytes) || writer.bit_count() == 0) {
        return false;
    }
    packed.bitCount = static_cast<std::uint16_t>(writer.bit_count());
    output = packed;
    return true;
}

/** @return The object's body bits, or zero for an object that cannot be written. */
[[nodiscard]] constexpr std::size_t body_bits(const Object& object) noexcept {
    if (object.generation <= 0 || object.componentCount > kComponentCapacity) {
        return 0;
    }
    std::size_t bits = kObjectBits + kComponentCountWidth;
    for (std::size_t i = 0; i < object.componentCount; ++i) {
        const auto& component = object.components[i];
        if (component.bitCount == 0 || component.bitCount > kComponentByteCapacity * kBitsPerByte) {
            return 0;
        }
        bits += fields::kPresenceWidth + kClassKeyWidth + component.bitCount;
    }
    return bits;
}

/** Writes one object body. @return False for an object `body_bits` refuses or a full writer. */
[[nodiscard]] inline bool write_body(encoding::bits::Writer& writer,
                                     const Object& object) noexcept {
    const auto expected = body_bits(object);
    if (expected == 0) {
        return false;
    }
    const auto start = writer.bit_count();
    const std::array<fields::Field, 5> head{{
        {static_cast<std::uint32_t>(object.generation) + fields::kSigned32Bias,
         fields::kSigned32Width},
        {fields::kSigned32Bias, fields::kSigned32Width}, // signed zero
        {object.active ? 1U : 0U, fields::kBoolWidth},
        {0, fields::kBoolWidth},
        {fields::kSigned32Bias, fields::kSigned32Width}, // signed zero
    }};
    const std::array<fields::Field, 5> transform{{
        {0, kTransformWordWidth},
        {0, kTransformWordWidth},
        {0, kTransformWordWidth},
        {0, fields::kBoolWidth},
        {object.componentCount, kComponentCountWidth},
    }};
    bool encoded = fields::write_fields(writer, head) && fields::write_absent_client_ref(writer)
                   && fields::write_fields(writer, transform);
    for (std::size_t i = 0; encoded && i < object.componentCount; ++i) {
        const auto& component = object.components[i];
        encoded = writer.write(1, fields::kPresenceWidth)
                  && writer.write(component.classKey, kClassKeyWidth);
        std::size_t remaining = component.bitCount;
        for (std::size_t byte = 0; encoded && remaining != 0; ++byte) {
            const auto width =
                static_cast<std::uint8_t>(remaining < kBitsPerByte ? remaining : kBitsPerByte);
            const auto value = std::to_integer<std::uint8_t>(component.body[byte]);
            encoded =
                writer.write(static_cast<std::uint8_t>(value >> (kBitsPerByte - width)), width);
            remaining -= width;
        }
    }
    return encoded && writer.bit_count() - start == expected;
}

} // namespace sunrise::middleware::bap::activity_message::match_object
