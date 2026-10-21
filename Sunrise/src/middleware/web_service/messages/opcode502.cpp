#include "opcode502.h"

#include "../../encoding/bit_reader.h"

namespace sunrise::middleware::web_service::messages::opcode502 {
namespace {

/** The request is one unbiased 64-bit character id with no presence bit. */
constexpr std::uint8_t kCharacterSoidWidth = 64;
/** Those 64 bits need 8 payload bytes. */
constexpr std::size_t kMinimumPayloadSize = 8;

} // namespace

/**
 * Parses the delete-character request: one 64-bit character id.
 * @param message Parsed Web Service envelope.
 * @param request Receives the targeted character id.
 * @return False when the opcode differs or the id is short.
 */
bool parse_request(const Message& message, Request& request) noexcept {
    request = {};
    if (message.opcode != kOpcode || message.payload.size() < kMinimumPayloadSize) {
        return false;
    }
    encoding::bits::Reader reader(message.payload);
    return reader.read(kCharacterSoidWidth, request.characterSoid);
}

} // namespace sunrise::middleware::web_service::messages::opcode502
