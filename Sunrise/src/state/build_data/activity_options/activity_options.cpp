#include <Windows.h>

#include "../../../middleware/content/packages/tables/activity_settings.h"
#include "runtime.h"

namespace sunrise::state::build_data::activity_options {
namespace {
namespace keys = middleware::content::packages::tables::activity_settings;
SRWLOCK g_lock = SRWLOCK_INIT;
std::array<Option, kOptionCapacity> g_options{};
std::size_t g_count = 0;
} // namespace

bool publish(std::span<const Option> options) noexcept {
    if (options.empty() || options.size() > kOptionCapacity) {
        return false;
    }
    for (const Option& option : options) {
        if (option.variantCount > kVariantCapacity
            || option.presentMask >= (1U << option.variantCount)) {
            return false;
        }
        if (option.presentMask != 0 && option.settingKey != keys::kTimeLimitKey
            && option.settingKey != keys::kScoreToWinKey) {
            return false;
        }
        for (std::size_t v = 0; v < kVariantCapacity; ++v) {
            if ((option.presentMask & (1U << v)) == 0 && option.values[v] != 0) {
                return false;
            }
        }
    }
    AcquireSRWLockExclusive(&g_lock);
    const bool fresh = g_count == 0;
    if (fresh) {
        for (std::size_t i = 0; i < options.size(); ++i) {
            g_options[i] = options[i];
        }
        g_count = options.size();
    }
    ReleaseSRWLockExclusive(&g_lock);
    return fresh;
}

bool resolve(std::span<const std::uint16_t> pairs,
             std::uint32_t key,
             std::int32_t& value) noexcept {
    if (pairs.size() > kPairCapacity
        || (key != keys::kTimeLimitKey && key != keys::kScoreToWinKey)) {
        return false;
    }
    bool found = false;
    bool valid = true;
    std::int32_t selected = 0;
    AcquireSRWLockShared(&g_lock);
    for (const std::uint16_t pair : pairs) {
        const int ordinal = static_cast<int>(pair >> kPairVariantWidth) - kPairBias;
        const int variant = static_cast<int>(pair & kPairVariantMask) - kPairBias;
        if (pair > kMaximumPair || ordinal < 0 || static_cast<std::size_t>(ordinal) >= g_count) {
            valid = false;
            break;
        }
        const Option& option = g_options[static_cast<std::size_t>(ordinal)];
        // Native 52B3D0 ignores absent or out-of-range variants.
        if (variant < 0 || variant >= option.variantCount || option.settingKey != key
            || (option.presentMask & (1U << variant)) == 0) {
            continue;
        }
        selected = option.values[static_cast<std::size_t>(variant)];
        found = true;
    }
    ReleaseSRWLockShared(&g_lock);
    if (!valid || !found) {
        return false;
    }
    value = selected;
    return true;
}
} // namespace sunrise::state::build_data::activity_options
