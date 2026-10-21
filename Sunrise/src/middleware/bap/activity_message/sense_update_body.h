#pragma once

#include <cstddef>
#include <cstdint>

#include "../../encoding/bit_reader.h"
#include "sense_update.h"

// Shared by the msg-6 packet framing and the per-schema Sense body readers.

namespace sunrise::middleware::bap::activity_message::sense_update::body {

/** How far one body decode got. */
enum class NativeStatus : std::uint8_t { complete, unsupported, unsafeCount, malformed };

/** Bit reader with its own budget, so one object cannot consume the whole packet. */
class Reader final {
public:
    Reader(encoding::bits::Reader& source, std::size_t budget, std::size_t total) noexcept
        : source_(source), left_(budget), total_(total) {}
    [[nodiscard]] bool read(std::uint8_t width, std::uint64_t& value) noexcept {
        if (width > left_ || !source_.read(width, value)) {
            return false;
        }
        left_ -= width;
        return true;
    }
    [[nodiscard]] bool skip(std::size_t width) noexcept {
        if (width > left_ || !source_.skip(width)) {
            return false;
        }
        left_ -= width;
        return true;
    }
    [[nodiscard]] std::size_t left() const noexcept {
        return left_;
    }
    [[nodiscard]] std::size_t position() const noexcept {
        return total_ - source_.remaining_bits();
    }

private:
    encoding::bits::Reader& source_;
    std::size_t left_{};
    std::size_t total_{};
};

/** Collects decoded values into the packet, dropping them once the object's room runs out. */
class Values final {
public:
    Values(DecodedPacket& packet, DecodedObject* object) noexcept
        : packet_(packet), object_(object) {}
    /** Retains one field value, marking truncation when the packet's storage is full. */
    void put(std::uint32_t schema,
             std::uint16_t ordinal,
             std::uint32_t occurrence,
             std::uint32_t at,
             std::uint8_t width,
             ValueKind kind,
             std::uint64_t raw,
             std::int64_t signedRaw,
             float real,
             bool present) noexcept {
        if (object_ == nullptr) {
            return;
        }
        if (packet_.valueCount == packet_.values.size()) {
            packet_.valuesTruncated = true;
            return;
        }
        DecodedValue& value = packet_.values[packet_.valueCount++];
        value.schemaRow = schema;
        value.fieldRow = ordinal;
        value.fieldOrdinal = ordinal;
        value.occurrence = occurrence;
        value.bitOffset = at;
        value.width = width;
        value.kind = kind;
        value.unsignedValue = raw;
        value.signedValue = signedRaw;
        value.realValue = real;
        value.present = present;
    }

private:
    DecodedPacket& packet_;
    DecodedObject* object_{};
};

/**
 * Decodes one root body of the given schema within the reader's budget.
 * @return How far it got; any status but malformed leaves the group skippable.
 */
[[nodiscard]] NativeStatus
decode_body(std::uint32_t schema, Reader& reader, Values& values) noexcept;

} // namespace sunrise::middleware::bap::activity_message::sense_update::body
