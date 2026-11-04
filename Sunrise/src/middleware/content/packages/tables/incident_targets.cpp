#include "incident_targets.h"

#include <algorithm>
#include <bit>
#include <bitset>
#include <limits>

#include "../../../bap/activity_message/incident.h"
#include "../../../encoding/bit_reader.h"
#include "../schema/generated/tag_classes_808077.h"
#include "../schema/generated/tag_classes_808087.h"
#include "definition_index_table.h"
#include "field_reader.h"

namespace sunrise::middleware::content::packages::tables::incident_targets {
namespace {
namespace generated = schema::generated;
using Bytes = std::span<const std::byte>;

#pragma pack(push, 1)
/** The last four bytes of a sobject row's unnamed `gap14`: the target's group and its node. */
struct TargetPlacement final {
    std::array<std::byte, 12> unused;
    std::int16_t group;
    std::int16_t node;
};
/** One presentation node; the layout names none of its fields. */
struct NodeRow final {
    std::uint16_t target;
    std::array<std::byte, 2> unused;
    std::uint16_t firstChild;
    std::uint16_t childCount;
};
#pragma pack(pop)
static_assert(sizeof(TargetPlacement) == sizeof(generated::SobjectRow::gap14));
static_assert(sizeof(NodeRow) == generated::C808077c7::kSize);

/**
 * Catalog root: the target rows, then the presentation groups. A target row is one sobject row
 * (`kSobjectRowStride`): its name hash, then its group, its node in that group and its type code.
 */
constexpr std::size_t kTargetList = offsetof(generated::C80807c9b, incidentGlobals.hAd7fd470);
constexpr std::uint32_t kTargetRowClass = generated::SobjectRow::kClass;
constexpr std::size_t kGroupList = offsetof(generated::C80807c9b, incidentGlobals.hDe0f4c5e);
constexpr std::uint32_t kGroupRowClass = generated::C80807739::kClass;
constexpr std::size_t kGroupRowStride = sizeof(generated::C80807739);
// Tag layout: the group and node ordinals sit in the sobject row's unnamed gap.
constexpr std::size_t kTargetGroupOffset =
    offsetof(generated::SobjectRow, gap14) + offsetof(TargetPlacement, group);
constexpr std::size_t kTargetNodeOffset =
    offsetof(generated::SobjectRow, gap14) + offsetof(TargetPlacement, node);
/** The installed payload type codes run from -1 (none) to 49. */
constexpr std::int8_t kMaximumTargetType = 49;
/**
 * The row's type word holds an 8-bit code with its upper bytes clear: the installed bank stores
 * "none" as 0x000000FF (rows 795, 4690 and 5375), never as a sign-extended word.
 */
constexpr std::uint32_t kTypeWordMaximum = (std::numeric_limits<std::uint8_t>::max)();
/** A group row's node array: each node names its target, first child and child count. */
constexpr std::size_t kGroupNodeList = offsetof(generated::C80807739, members);
constexpr std::uint32_t kNodeRowClass = generated::C808077c7::kClass;
constexpr std::size_t kNodeRowStride = sizeof(generated::C808077c7);
constexpr std::size_t kNodeTargetOffset = offsetof(NodeRow, target);
constexpr std::size_t kNodeFirstChildOffset = offsetof(NodeRow, firstChild);
constexpr std::size_t kNodeChildCountOffset = offsetof(NodeRow, childCount);

/** The selector shares the incident envelope's byte bound. */
constexpr std::size_t kSelectorMaximum = bap::activity_message::incident::kSelectorMaximum;
/** Children are selected in chunks of eight, each behind its own presence bit. */
constexpr std::size_t kChildChunk = 8;
constexpr std::uint8_t kChunkPresenceWidth = 1;
/** Trailing padding completes the last selector byte. */
constexpr std::size_t kMaximumPaddingBits = 7;

/** Reads one nonempty typed table through the shared `read_array` reader. */
bool array(Bytes bytes,
           std::size_t descriptor,
           std::uint32_t cls,
           std::size_t stride,
           std::size_t maximum,
           std::size_t& start,
           std::size_t& count) noexcept {
    Array rows{};
    if (!read_array(bytes, descriptor, cls, stride, rows) || rows.count == 0
        || rows.count > maximum) {
        return false;
    }
    start = rows.dataOffset;
    count = static_cast<std::size_t>(rows.count);
    return true;
}
} // namespace

bool parse(Bytes bytes, Catalog& output) noexcept {
    Catalog result{};
    std::size_t rows{}, count{}, groupRows{}, groups{};
    if (!array(bytes,
               kTargetList,
               kTargetRowClass,
               kSobjectRowStride,
               result.targets.size(),
               rows,
               count)
        || !array(bytes,
                  kGroupList,
                  kGroupRowClass,
                  kGroupRowStride,
                  result.groups.size(),
                  groupRows,
                  groups)) {
        return false;
    }
    result.targetCount = static_cast<std::uint16_t>(count);
    result.groupCount = static_cast<std::uint8_t>(groups);
    for (std::size_t i = 0; i < count; ++i) {
        const auto row = rows + (i * kSobjectRowStride);
        auto& target = result.targets[i];
        // The whole type word is read and range-checked before it is narrowed to its code.
        std::uint32_t typeWord{};
        if (!read(bytes, row + kSobjectNameHashOffset, target.key)
            || !read(bytes, row + kTargetGroupOffset, target.group)
            || !read(bytes, row + kTargetNodeOffset, target.node)
            || !read(bytes, row + kSobjectTypeCodeOffset, typeWord) || typeWord > kTypeWordMaximum
            || target.group < -1
            || (target.group >= 0 && static_cast<std::size_t>(target.group) >= groups)) {
            return false;
        }
        const auto type = std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(typeWord));
        if (type < -1 || type > kMaximumTargetType) {
            return false;
        }
        target.type = type;
    }
    for (std::size_t g = 0; g < groups; ++g) {
        std::size_t nodes{}, nodeCount{};
        if (!array(bytes,
                   groupRows + (g * kGroupRowStride) + kGroupNodeList,
                   kNodeRowClass,
                   kNodeRowStride,
                   kGroupNodeCapacity,
                   nodes,
                   nodeCount)
            || nodeCount > result.nodes.size() - result.nodeCount) {
            return false;
        }
        auto& group = result.groups[g];
        group = {result.nodeCount, static_cast<std::uint16_t>(nodeCount)};
        for (std::size_t n = 0; n < nodeCount; ++n) {
            auto& node = result.nodes[result.nodeCount + n];
            const auto row = nodes + (n * kNodeRowStride);
            if (!read(bytes, row + kNodeTargetOffset, node.target) || node.target >= count
                || !read(bytes, row + kNodeFirstChildOffset, node.firstChild)
                || !read(bytes, row + kNodeChildCountOffset, node.childCount)
                || (node.childCount != 0
                    && (node.firstChild <= n || node.firstChild >= nodeCount
                        || node.childCount > nodeCount - node.firstChild))) {
                return false;
            }
            const auto& target = result.targets[node.target];
            if (target.type < 0 || target.group != static_cast<std::int16_t>(g)
                || target.node != static_cast<std::int16_t>(n)) {
                return false;
            }
        }
        result.nodeCount += static_cast<std::uint16_t>(nodeCount);
    }
    output = result;
    return true;
}

bool expand(const Catalog& catalog, Bytes selector, Selection& output) noexcept {
    if (selector.empty() || selector.size() > kSelectorMaximum || catalog.groupCount == 0
        || catalog.groupCount > catalog.groups.size() || catalog.nodeCount > catalog.nodes.size()
        || catalog.targetCount > catalog.targets.size()) {
        return false;
    }
    encoding::bits::Reader reader(selector);
    std::uint64_t value{};
    const auto width =
        static_cast<std::uint8_t>(std::bit_width(static_cast<unsigned>(catalog.groupCount)));
    if (!reader.read(width, value) || value >= catalog.groupCount) {
        return false;
    }
    const auto& group = catalog.groups[static_cast<std::size_t>(value)];
    if (group.nodeCount == 0 || group.nodeCount > kGroupNodeCapacity
        || group.firstNode >= catalog.nodeCount
        || group.nodeCount > catalog.nodeCount - group.firstNode) {
        return false;
    }
    std::bitset<kGroupNodeCapacity> active{};
    active.set(0);
    Selection result{};
    for (std::size_t i = 0; i < group.nodeCount; ++i) {
        if (!active[i]) {
            continue;
        }
        const auto& node = catalog.nodes[group.firstNode + i];
        if (result.count == result.targets.size() || node.target >= catalog.targetCount
            || catalog.targets[node.target].type < 0
            || (node.childCount != 0
                && (node.firstChild <= i || node.firstChild >= group.nodeCount
                    || node.childCount > group.nodeCount - node.firstChild))) {
            return false;
        }
        result.targets[result.count++] = node.target;
        for (std::size_t child = 0; child < node.childCount; child += kChildChunk) {
            const auto bits = static_cast<std::uint8_t>(
                std::min<std::size_t>(kChildChunk, node.childCount - child));
            if (!reader.read(kChunkPresenceWidth, value)) {
                return false;
            }
            if (value == 0) {
                continue;
            }
            if (!reader.read(bits, value)) {
                return false;
            }
            for (std::size_t bit = 0; bit < bits; ++bit) {
                if ((value & (std::uint64_t{1} << bit)) != 0) {
                    active.set(node.firstChild + child + bit);
                }
            }
        }
    }
    const auto padding = reader.remaining_bits();
    if (padding > kMaximumPaddingBits || !reader.read(static_cast<std::uint8_t>(padding), value)
        || value != 0) {
        return false;
    }
    output = result;
    return true;
}

} // namespace sunrise::middleware::content::packages::tables::incident_targets
