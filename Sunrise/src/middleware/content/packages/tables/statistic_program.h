#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "../schema/generated/tag_classes_7f0007.h"
#include "../schema/generated/tag_classes_80809a.h"

namespace sunrise::middleware::content::packages::tables::statistic_program {

/** Installed scoreboard config class; its fixed root is `kScoreboardRootSize` bytes. */
inline constexpr std::uint32_t kScoreboardClass = schema::generated::C80809a12::kClass;
inline constexpr std::size_t kScoreboardRootSize = sizeof(schema::generated::C80809a12);
// Tag layout: each fixed statistic list follows its count word, which the layout leaves unnamed.
/** Player statistic list: its count, then rows of `kStatisticStride` bytes. */
inline constexpr std::size_t kPlayerStatisticCountOffset =
    offsetof(schema::generated::C80809a12, gap224);
inline constexpr std::size_t kPlayerStatisticsOffset =
    offsetof(schema::generated::C80809a12, unk228);
/** Team statistic list consumed by FDA0C0/FDA960. */
inline constexpr std::size_t kTeamStatisticCountOffset =
    offsetof(schema::generated::C80809a12, gap728);
inline constexpr std::size_t kTeamStatisticsOffset = offsetof(schema::generated::C80809a12, unk72c);
/** The separate global statistic list. */
inline constexpr std::size_t kGlobalStatisticCountOffset =
    offsetof(schema::generated::C80809a12, gap40);
inline constexpr std::size_t kGlobalStatisticsOffset =
    offsetof(schema::generated::C80809a12, unk44);
/** One statistic row: the statistic key, then the column metadata. All three lists share it. */
inline constexpr std::size_t kStatisticStride = sizeof(schema::generated::G80809a12228);
inline constexpr std::size_t kStatisticKeyOffset = offsetof(schema::generated::G80809a12228, unk0);
// Tag layout: the column metadata word is the row's unnamed gap.
inline constexpr std::size_t kStatisticMetadataOffset =
    offsetof(schema::generated::G80809a12228, gapC);
static_assert(sizeof(schema::generated::G80809a1244) == kStatisticStride
              && sizeof(schema::generated::G80809a1272c) == kStatisticStride);
/** Statistic list capacities of the native per-player, per-faction and global score arrays. */
inline constexpr std::size_t kPlayerStatisticCapacity =
    std::tuple_size_v<decltype(schema::generated::C80809a12::unk228)>;
inline constexpr std::size_t kTeamStatisticCapacity =
    std::tuple_size_v<decltype(schema::generated::C80809a12::unk72c)>;
inline constexpr std::size_t kGlobalStatisticCapacity =
    std::tuple_size_v<decltype(schema::generated::C80809a12::unk44)>;
/** FNV-1 of `score`, the statistic that leads both lists. */
inline constexpr std::uint32_t kScoreStatistic = 0x8EF21B81U;
/** The low metadata byte is the column format; format 3 is a time column. */
inline constexpr std::uint32_t kColumnFormatMask = 0xFFU;
inline constexpr std::uint32_t kTimeColumnFormat = 3;

/** Every VM value is a four-lane float vector. */
inline constexpr std::size_t kVectorLanes = 4;
using Vector = std::array<float, kVectorLanes>;

/** Program storage: code bytes, float4 constants and variable keys of one expression. */
inline constexpr std::size_t kCodeCapacity = 32;
inline constexpr std::size_t kConstantCapacity = 8;
inline constexpr std::size_t kVariableCapacity = 16;
/** Expression rows one scoreboard config holds: event programs, then computed columns. */
inline constexpr std::size_t kEventCapacity = 64;
inline constexpr std::size_t kComputedCapacity = 32;

/** Owned copy of one installed event or computed-column expression, in authored order. */
struct Program final {
    std::uint32_t eventKey{};
    std::uint32_t roleKey{};
    std::uint32_t outputKey{};
    std::uint32_t flags{};
    std::array<std::uint8_t, kCodeCapacity> code{};
    std::array<Vector, kConstantCapacity> constants{};
    std::array<std::uint32_t, kVariableCapacity> variables{};
    std::uint8_t codeCount{};
    std::uint8_t constantCount{};
    std::uint8_t variableCount{};
};

struct Catalog final {
    std::array<std::uint32_t, kPlayerStatisticCapacity> playerColumns{};
    std::array<std::uint32_t, kPlayerStatisticCapacity> playerColumnMetadata{};
    std::array<std::uint32_t, kTeamStatisticCapacity> teamColumns{};
    std::array<Program, kEventCapacity> events{};
    std::array<Program, kComputedCapacity> computed{};
    std::uint8_t playerColumnCount{};
    std::uint8_t teamColumnCount{};
    std::uint8_t eventCount{};
    std::uint8_t computedCount{};
};

/** Reject unsupported instructions, invalid operands and unbalanced/outputless programs. */
[[nodiscard]] bool valid(const Program& program) noexcept;

/** Decode a complete installed scoreboard config; failure leaves output unchanged. */
[[nodiscard]] bool parse(std::span<const std::byte> bytes, Catalog& output) noexcept;

/**
 * Runs one program on native 3B7A80's float4 instruction subset; inputs follow the authored
 * variable keys. Every installed ratio divides by at least one, so a smaller divisor fails, as
 * does any nonfinite result; `output` is then unchanged.
 */
[[nodiscard]] bool
evaluate(const Program& program, std::span<const Vector> inputs, Vector& output) noexcept;

} // namespace sunrise::middleware::content::packages::tables::statistic_program
