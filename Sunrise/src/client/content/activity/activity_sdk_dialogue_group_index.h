#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace sunrise::client::content::activity::sdk_generation::dialogue_group_index {

/** One cue definition's delayed-request expiration window, in seconds. */
struct Definition final {
    std::uint32_t hash{};
    float authoredWindowSeconds{};

    bool operator==(const Definition&) const = default;
};

/** Reads every authored cue definition in bank order, including empty windows. */
[[nodiscard]] bool definitions(std::span<const std::byte> bytes,
                               std::vector<Definition>& output) noexcept;

} // namespace sunrise::client::content::activity::sdk_generation::dialogue_group_index
