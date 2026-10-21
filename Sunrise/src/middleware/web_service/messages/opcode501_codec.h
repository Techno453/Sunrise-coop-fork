#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "../web_service_envelope.h"

namespace sunrise::middleware::web_service::messages::opcode501 {

/** Web Service opcode for the create-character request. */
inline constexpr std::uint16_t kOpcode = 501;

/**
 * Encodes the create-character response: the status pair then the character object id.
 * The client reads a refusal from the status code, not from the id.
 * @param message Parsed request whose envelope fields are echoed.
 * @param status Refusal code, and the Family-4 revision that carries the new character.
 * @param characterSoid The new character's id; zero on a refusal.
 * @param output Caller-owned svc-11 response-body storage.
 * @param written Receives encoded response-body bytes.
 * @return True when the fixed response fits the output buffer.
 */
[[nodiscard]] bool encode_response(const Message& message,
                                   const StatusResponse& status,
                                   std::uint64_t characterSoid,
                                   std::span<std::byte> output,
                                   std::size_t& written) noexcept;

} // namespace sunrise::middleware::web_service::messages::opcode501
