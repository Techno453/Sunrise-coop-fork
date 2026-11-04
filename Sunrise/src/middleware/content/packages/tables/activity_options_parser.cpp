#include "../schema/generated/tag_classes_80807d.h"
#include "activity_options.h"
#include "activity_settings.h"
#include "definition_index_table.h"
#include "field_reader.h"

namespace sunrise::middleware::content::packages::tables::activity_options {
namespace {
namespace keys = activity_settings;
namespace generated = schema::generated;
/** The option table's row list. */
constexpr std::size_t kTableRows = offsetof(generated::C80807d6b, table);
/** Option rows: the option's setting key, then its variant tag list. */
constexpr std::uint32_t kRowClass = generated::C80807d71::kClass;
constexpr std::size_t kRowSize = sizeof(generated::C80807d71);
constexpr std::size_t kRowSettingKeyOffset = offsetof(generated::C80807d71, unk4);
constexpr std::size_t kRowVariantList = offsetof(generated::C80807d71, values);
/** A variant names the setting list that holds its value; an all-ones tag names none. */
constexpr std::uint32_t kTagArrayClass = generated::C80807d73::kClass;
constexpr std::size_t kTagStride = sizeof(generated::C80807d73);
constexpr std::uint32_t kAbsentVariantTag = 0xFFFFFFFFU;
constexpr std::uint32_t kSettingListClass = generated::C80807d82::kClass;
/** A setting list's typed value references. */
constexpr std::size_t kSettingListValues = offsetof(generated::C80807d82, options.options);
/** Native 52B3D0/A55110 map capacity. */
constexpr std::size_t kMaximumSettings = 350;

/** Reads one nonempty typed table through the shared `read_array` reader. */
bool rows_of(std::span<const std::byte> blob,
             std::size_t at,
             std::uint32_t cls,
             std::size_t stride,
             Array& rows) noexcept {
    return read_array(blob, at, cls, stride, rows) && rows.count != 0;
}

bool integer_value(std::span<const std::byte> blob,
                   std::uint32_t key,
                   std::int32_t& value,
                   bool& found) noexcept {
    Array settings{};
    if (!rows_of(blob,
                 kSettingListValues,
                 keys::kSettingReferenceClass,
                 keys::kSettingReferenceStride,
                 settings)
        || settings.count > kMaximumSettings) {
        return false;
    }
    found = false;
    for (std::size_t i = 0; i < settings.count; ++i) {
        const std::size_t at = settings.dataOffset + (i * keys::kSettingReferenceStride);
        std::size_t body = 0;
        std::uint32_t actualKey = 0;
        if (!relative(blob, at, body) || body < keys::kSettingClassBefore
            || !read(blob, body + keys::kSettingKeyOffset, actualKey)) {
            return false;
        }
        if (actualKey != key) {
            continue;
        }
        std::uint32_t cls = 0;
        if (!read(blob, body - keys::kSettingClassBefore, cls) || cls != keys::kIntegerClass
            || !read(blob, body + keys::kSettingPayloadOffset, value)) {
            return false;
        }
        found = true; // Later setters replace earlier ones, as in native A55110.
    }
    return true;
}
} // namespace

bool parse(std::span<const std::byte> table,
           void* context,
           ReadTag readTag,
           Catalog& output) noexcept {
    Array rows{};
    if (readTag == nullptr || !rows_of(table, kTableRows, kRowClass, kRowSize, rows)
        || rows.count > kOptionCapacity) {
        return false;
    }
    Catalog parsed{};
    parsed.count = static_cast<std::uint16_t>(rows.count);
    for (std::size_t i = 0; i < rows.count; ++i) {
        const std::size_t row = rows.dataOffset + (i * kRowSize);
        Option& option = parsed.options[i];
        std::uint64_t count = 0;
        if (!read(table, row + kRowSettingKeyOffset, option.settingKey)
            || !read(table, row + kRowVariantList, count) || count > kVariantCapacity) {
            return false;
        }
        option.variantCount = static_cast<std::uint8_t>(count);
        if (count == 0) {
            continue;
        }
        Array variants{};
        if (!rows_of(table, row + kRowVariantList, kTagArrayClass, kTagStride, variants)) {
            return false;
        }
        if (option.settingKey != keys::kTimeLimitKey && option.settingKey != keys::kScoreToWinKey) {
            continue;
        }
        for (std::size_t v = 0; v < count; ++v) {
            std::uint32_t tag = 0;
            if (!read(table, variants.dataOffset + (v * kTagStride), tag)) {
                return false;
            }
            if (tag == kAbsentVariantTag) {
                continue;
            }
            if (package_of(tag) == kAbsentPackageId) {
                return false;
            }
            std::span<const std::byte> child{};
            std::uint32_t cls = 0;
            bool found = false;
            if (!readTag(context, tag, child, cls) || cls != kSettingListClass
                || !integer_value(child, option.settingKey, option.values[v], found)) {
                return false;
            }
            if (found) {
                option.presentMask |= static_cast<std::uint16_t>(1U << v);
            }
        }
    }
    output = parsed;
    return true;
}
} // namespace sunrise::middleware::content::packages::tables::activity_options
