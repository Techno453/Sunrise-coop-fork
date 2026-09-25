#include "scriptable_catalog_container_index.h"

#include <algorithm>
#include <memory>
#include <span>
#include <string_view>
#include <tuple>

#include "../../../middleware/content/packages/tables/component_container_reader.h"
#include "../spawn_sets/spawn_set_catalog_builder.h"

namespace sunrise::client::content::activity::scriptables::internal {
namespace {

namespace package_reader = middleware::content::packages::reader;
namespace tables = middleware::content::packages::tables;

/** Bounds the retained sweep so a malformed install cannot grow the index without limit. */
constexpr std::size_t kEntryCapacity = 262'144;

struct SweepContext final {
    std::vector<ContainerIndexEntry>* rows{};
    ContainerIndexCancelCheck cancel{};
    bool stemsComplete{true};
    bool failed{};
};

/** One container a bubble definition names, before its wide reference is resolved. */
struct BubbleLoad final {
    std::uint32_t handle{};
    std::uint64_t key{};
    std::size_t definition{};
};

struct KeyContext final {
    const std::vector<std::uint64_t>* wanted{};
    std::vector<package_reader::Reference>* rows{};
};

[[nodiscard]] bool cancelled(ContainerIndexCancelCheck cancel) noexcept {
    return cancel != nullptr && cancel();
}

/** Keeps one swept tag and the normalized stem of the package family holding it. */
[[nodiscard]] bool collect_entry(void* opaque, const package_reader::ClassEntry& entry) noexcept {
    auto& context = *static_cast<SweepContext*>(opaque);
    if (context.rows == nullptr || cancelled(context.cancel)) {
        context.failed = true;
        return false;
    }
    if (context.rows->size() >= kEntryCapacity) {
        context.failed = true;
        return false;
    }
    ContainerIndexEntry row{};
    row.tag = entry.tag;
    row.stemValid = spawn_sets::normalize_stem(entry.packageFamily, row.stem, row.stemLength);
    if (!row.stemValid) {
        context.stemsComplete = false;
    }
    try {
        context.rows->push_back(row);
    } catch (...) {
        context.failed = true;
        return false;
    }
    return true;
}

/** Keeps every hash64 reference row whose key a bubble definition names. */
[[nodiscard]] bool collect_key(void* opaque, const package_reader::Reference& reference) noexcept {
    const auto& context = *static_cast<KeyContext*>(opaque);
    if (!std::binary_search(context.wanted->begin(), context.wanted->end(), reference.key)) {
        return true;
    }
    try {
        context.rows->push_back(reference);
    } catch (...) {
        return false;
    }
    return true;
}

/** @return True when two rows carry the same normalized stem. */
[[nodiscard]] bool same_stem(const ContainerIndexEntry& first,
                             const ContainerIndexEntry& second) noexcept {
    return std::string_view(first.stem.data(), first.stemLength)
           == std::string_view(second.stem.data(), second.stemLength);
}

/** Orders rows by stem, then tag, so each stem's loads sit together in a stable order. */
[[nodiscard]] bool stem_then_tag(const ContainerIndexEntry& first,
                                 const ContainerIndexEntry& second) noexcept {
    const std::string_view firstStem(first.stem.data(), first.stemLength);
    const std::string_view secondStem(second.stem.data(), second.stemLength);
    return std::tie(firstStem, first.tag) < std::tie(secondStem, second.tag);
}

/** Reads every bubble definition's container array into unresolved loads. */
[[nodiscard]] bool read_bubble_loads(const package_reader::Source& source,
                                     const std::vector<ContainerIndexEntry>& definitions,
                                     std::vector<BubbleLoad>& loads,
                                     ContainerIndexCancelCheck cancel) {
    auto scratch = std::make_unique<package_reader::Scratch>();
    std::vector<std::byte> bytes{};
    bool read = true;
    for (std::size_t definition = 0; read && definition < definitions.size(); ++definition) {
        std::uint32_t classId = 0;
        tables::Array containers{};
        read = !cancelled(cancel)
               && package_reader::read_tag(
                   source, *scratch, definitions[definition].tag, bytes, classId)
               && classId == tables::kBubbleDefinitionClass
               && tables::bubble_containers(bytes, containers);
        for (std::uint64_t index = 0; read && index < containers.count; ++index) {
            BubbleLoad load{};
            load.definition = definition;
            read = tables::bubble_container_at(bytes, containers, index, load.handle, load.key);
            if (read) {
                loads.push_back(load);
            }
        }
    }
    package_reader::close_files(*scratch);
    return read;
}

/**
 * Lists every container a bubble definition loads from another package family.
 * A wide reference names its container by tag handle or, far more often, by hash64 key alone.
 * A key that no installed package resolves to exactly one container names nothing loadable.
 * @return True when every sweep and read worked.
 */
[[nodiscard]] bool collect_bubble_loads(const package_reader::Source& source,
                                        ContainerIndex& output,
                                        ContainerIndexCancelCheck cancel) {
    std::vector<ContainerIndexEntry> definitions{};
    SweepContext sweep{&definitions, cancel};
    package_reader::ScanResult scan{};
    std::vector<BubbleLoad> loads{};
    if (!package_reader::scan_class_entries(
            source.directory, tables::kBubbleDefinitionClass, &collect_entry, &sweep, scan)
        || sweep.failed || !read_bubble_loads(source, definitions, loads, cancel)) {
        return false;
    }

    std::vector<std::uint64_t> wanted{};
    for (const BubbleLoad& load : loads) {
        if (load.handle == tables::kWideReferenceNoHandle) {
            wanted.push_back(load.key);
        }
    }
    std::sort(wanted.begin(), wanted.end());
    wanted.erase(std::unique(wanted.begin(), wanted.end()), wanted.end());
    std::vector<package_reader::Reference> keys{};
    KeyContext keyContext{&wanted, &keys};
    if (cancelled(cancel)
        || !package_reader::scan_references(source.directory, &collect_key, &keyContext)) {
        return false;
    }
    const auto fields = [](const package_reader::Reference& row) {
        return std::tie(row.key, row.tag, row.classId);
    };
    std::sort(keys.begin(), keys.end(), [&fields](const auto& first, const auto& second) {
        return fields(first) < fields(second);
    });
    keys.erase(std::unique(keys.begin(),
                           keys.end(),
                           [&fields](const auto& first, const auto& second) {
                               return fields(first) == fields(second);
                           }),
               keys.end());

    std::vector<ContainerIndexEntry> byTag = output.entries;
    std::sort(byTag.begin(), byTag.end(), [](const auto& first, const auto& second) {
        return first.tag < second.tag;
    });
    for (const BubbleLoad& load : loads) {
        std::uint32_t tag = load.handle;
        if (tag == tables::kWideReferenceNoHandle) {
            const auto [first, last] = std::equal_range(
                keys.begin(),
                keys.end(),
                package_reader::Reference{load.key, 0, 0},
                [](const package_reader::Reference& left, const package_reader::Reference& right) {
                    return left.key < right.key;
                });
            // Two different rows under one key leave the reference ambiguous.
            if (last - first != 1 || first->classId != tables::kContainerClass) {
                continue;
            }
            tag = first->tag;
        }
        const auto container = std::lower_bound(
            byTag.begin(),
            byTag.end(),
            tag,
            [](const ContainerIndexEntry& row, std::uint32_t value) { return row.tag < value; });
        const ContainerIndexEntry& definition = definitions[load.definition];
        // A container of the definition's own family is already listed under that stem.
        if (container == byTag.end() || container->tag != tag || !definition.stemValid
            || (container->stemValid && same_stem(*container, definition))) {
            continue;
        }
        ContainerIndexEntry row = definition;
        row.tag = tag;
        output.bubbleLoads.push_back(row);
    }
    std::sort(output.bubbleLoads.begin(), output.bubbleLoads.end(), &stem_then_tag);
    output.bubbleLoads.erase(
        std::unique(output.bubbleLoads.begin(),
                    output.bubbleLoads.end(),
                    [](const ContainerIndexEntry& first, const ContainerIndexEntry& second) {
                        return first.tag == second.tag && same_stem(first, second);
                    }),
        output.bubbleLoads.end());
    return true;
}

} // namespace

/** Sweeps the install once for the container class and keeps each tag with its stem. */
bool build_container_index(const package_reader::Source& source,
                           ContainerIndex& output,
                           ContainerIndexCancelCheck cancel) noexcept {
    output = {};
    output.stemsComplete = true;
    try {
        output.entries.reserve(4'096);
    } catch (...) {
        return false;
    }
    SweepContext context{&output.entries, cancel};
    package_reader::ScanResult scan{};
    const bool scanned = package_reader::scan_class_entries(
        source.directory, tables::kContainerClass, &collect_entry, &context, scan);
    output.stemsComplete = context.stemsComplete;
    if (context.failed || cancelled(cancel)) {
        return false;
    }
    bool loaded = false;
    try {
        loaded = collect_bubble_loads(source, output, cancel);
    } catch (...) {
        loaded = false;
    }
    if (cancelled(cancel)) {
        return false;
    }
    // A failed bubble sweep leaves the index open, so no consumer can take it as closed.
    if (!loaded) {
        output.bubbleLoads.clear();
    }
    output.complete = scanned && loaded;
    return true;
}

} // namespace sunrise::client::content::activity::scriptables::internal
