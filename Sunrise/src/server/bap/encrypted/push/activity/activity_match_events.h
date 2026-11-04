#pragma once
#include "internal.h"
namespace sunrise::server::bap::encrypted::push::activity {
// The BAP session lock serializes ingress and transport staging.
void queue_match_event(const ActivityClientBinding& source,
                       std::uint64_t member,
                       std::span<const std::byte> body) noexcept;
bool match_events_due(Session& session) noexcept;
bool append_match_events(Session& session,
                         Scratch& scratch,
                         std::array<std::byte, state::kBapNonceSize>& nonce,
                         std::size_t& written) noexcept;
void commit_match_events(Session& session) noexcept;
void discard_match_events(Session& session) noexcept;
} // namespace sunrise::server::bap::encrypted::push::activity
