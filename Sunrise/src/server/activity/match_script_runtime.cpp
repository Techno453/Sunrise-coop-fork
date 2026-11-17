#include "match_script_runtime.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>
#include <new>
#include <span>
#include <string_view>

#include "../../core/filesystem/path.h"
#include "../../core/logging/log.h"
#include "../../state/activity/definition.h"
#include "../../state/activity/match_modes/control.h"
#include "../../state/activity/match_modes/rumble.h"
#include "../../state/activity/match_state/script_policy.h"
#include "../../state/mods/mods_runtime.h"
#include "mission/mission_script_vm.h"

namespace sunrise::server::activity::match_scripts {
namespace {
namespace policy = state::activity::match_state::script_policy;
namespace vm = mission::lua_vm;

struct Program final {
    vm::Vm value{};
    std::unique_ptr<Program> next{};
};
struct Instance final {
    std::unique_ptr<Program> program{};
    policy::Binding binding{};
    bool reportedFailure{};
};

struct DeferredLine final {
    std::array<char, core::log::kLineCapacity> bytes{};
    std::size_t size{};
    core::log::Level level{};
};
/**
 * Script diagnostics are formatted into this queue rather than passed to `core::log::writef`:
 * `evaluate` runs inside State's exclusive transaction and writef writes at once, so the lines
 * wait here until the outermost `DeferredScope` has released that lock.
 */
struct DeferredWork final {
    std::unique_ptr<Program> retired{};
    // A State entry opens at most one VM and faults at most once. Nested projection
    // entries share this scope; keep bounded diagnostics even for a broken caller.
    std::array<DeferredLine, 16> lines{};
    std::size_t count{};
    std::size_t dropped{};
};
thread_local DeferredWork g_deferred;
void flush() noexcept {
    while (g_deferred.retired) {
        auto program = std::move(g_deferred.retired);
        g_deferred.retired = std::move(program->next);
        program.reset();
    }
    for (std::size_t i = 0; i < g_deferred.count; ++i) {
        const auto& line = g_deferred.lines[i];
        core::log::write(core::log::Channel::server, line.level, {line.bytes.data(), line.size});
    }
    if (g_deferred.dropped) {
        core::log::write(core::log::Channel::server,
                         core::log::Level::warn,
                         "ev=match_script stage=diagnostics result=overflow");
    }
    g_deferred.count = g_deferred.dropped = 0;
}
void retire(Instance& instance) noexcept {
    if (instance.program) {
        instance.program->next = std::move(g_deferred.retired);
        g_deferred.retired = std::move(instance.program);
    }
    instance.binding = {};
    instance.reportedFailure = false;
}
void queue_line(std::span<const char> text, core::log::Level level) noexcept {
    if (g_deferred.count < g_deferred.lines.size()) {
        auto& line = g_deferred.lines[g_deferred.count++];
        line.size = (std::min)(text.size(), line.bytes.size());
        std::copy_n(text.begin(), line.size, line.bytes.begin());
        line.level = level;
    } else {
        ++g_deferred.dropped;
    }
    if (policy::g_deferredDepth == 0) {
        flush();
    }
}
/** An abort line: the binding's three fields and a short reason name. */
constexpr std::size_t kAbortLineCapacity = 256;
void report_abort(const policy::Binding& binding, std::string_view reason) noexcept {
    std::array<char, kAbortLineCapacity> line{};
    const int size = std::snprintf(
        line.data(),
        line.size(),
        "ev=match_script stage=abort session=%llu generation=%llu mode=%08X reason=%.*s",
        static_cast<unsigned long long>(binding.sessionId),
        static_cast<unsigned long long>(binding.createdRevision),
        binding.modeKey,
        static_cast<int>(reason.size()),
        reason.data());
    if (size > 0 && static_cast<std::size_t>(size) < line.size()) {
        queue_line({line.data(), static_cast<std::size_t>(size)}, core::log::Level::warn);
    }
}

std::mutex g_lock;
std::array<Instance, state::activity::kSessionCapacity> g_instances{};
struct Source final {
    std::uint32_t modeKey{};
    /** File name in a `scripts\matches` folder. */
    std::wstring_view file;
    /** The same name for log lines. */
    std::string_view name;
    std::array<char, vm::kSourceByteCapacity> text{};
    std::size_t size{};
};
// Source identity is independent of runtime allocations and selected by the resolved mode key.
std::array g_sources{
    Source{state::activity::match_modes::rumble::kMode, L"rumble.lua", "rumble.lua"},
    Source{state::activity::match_modes::control::kMode, L"control.lua", "control.lua"}};
/** The library every mode's program runs first; no mode opens without it. */
Source g_library{0, L"common.lua", "common.lua"};
/** Holds a file while it is read and validated; only `initialize` reads files. */
std::array<char, vm::kSourceByteCapacity> g_fileBuffer{};
Source* source_for(std::uint32_t modeKey) noexcept {
    for (auto& source : g_sources) {
        if (source.modeKey == modeKey) {
            return &source;
        }
    }
    return nullptr;
}

void report(std::string_view stage,
            std::string_view result,
            const policy::Binding& binding = {},
            std::string_view error = {}) noexcept {
    std::array<char, core::log::kLineCapacity> line{};
    const int size = std::snprintf(line.data(),
                                   line.size(),
                                   "ev=match_script stage=%.*s result=%.*s session=%llu "
                                   "generation=%llu mode=%08X error=\"%.*s\"",
                                   static_cast<int>(stage.size()),
                                   stage.data(),
                                   static_cast<int>(result.size()),
                                   result.data(),
                                   static_cast<unsigned long long>(binding.sessionId),
                                   static_cast<unsigned long long>(binding.createdRevision),
                                   binding.modeKey,
                                   static_cast<int>(error.size()),
                                   error.empty() ? "" : error.data());
    if (size > 0 && static_cast<std::size_t>(size) < line.size()) {
        const bool success = result == "ready" || result.starts_with("loaded");
        queue_line({line.data(), static_cast<std::size_t>(size)},
                   success ? core::log::Level::info : core::log::Level::warn);
    }
}

/** Called under State's transaction lock. It never calls State or the Mission manager. */
bool evaluate(const policy::Binding& binding,
              const policy::Request& input,
              policy::Response& output) noexcept {
    const policy::DeferredScope deferred;
    const std::lock_guard lock(g_lock);
    const auto* source = source_for(binding.modeKey);
    if (source == nullptr || source->size == 0 || binding.slot >= g_instances.size()
        || binding.sessionId == 0 || binding.createdRevision == 0) {
        return false;
    }
    auto& instance = g_instances[binding.slot];
    if (instance.binding.sessionId != binding.sessionId
        || instance.binding.createdRevision != binding.createdRevision
        || instance.binding.modeKey != binding.modeKey) {
        retire(instance);
        instance.binding = binding;
        instance.program.reset(new (std::nothrow) Program);
        const auto status = instance.program
                                ? vm::open_match_policy(instance.program->value,
                                                        {g_library.text.data(), g_library.size},
                                                        {source->text.data(), source->size},
                                                        true)
                                : vm::OpenStatus::outOfMemory;
        if (status != vm::OpenStatus::ready) {
            vm::Snapshot snapshot{};
            if (instance.program) {
                vm::snapshot(instance.program->value, snapshot);
            }
            report("open", vm::status_name(status), binding, snapshot.lastError.data());
            instance.reportedFailure = true;
            return false;
        }
        report("open", "ready", binding);
    }
    if (!instance.program) {
        return false;
    }
    const auto status = vm::evaluate_match(instance.program->value, input, output);
    if (status != vm::CallStatus::committed && !instance.reportedFailure) {
        vm::Snapshot snapshot{};
        vm::snapshot(instance.program->value, snapshot);
        report("callback", vm::status_name(status), binding, snapshot.lastError.data());
        instance.reportedFailure = true;
    }
    return status == vm::CallStatus::committed;
}

void release(std::size_t slot, std::uint64_t sessionId, std::uint64_t revision) noexcept {
    const policy::DeferredScope deferred;
    const std::lock_guard lock(g_lock);
    if (slot < g_instances.size() && g_instances[slot].binding.sessionId == sessionId
        && g_instances[slot].binding.createdRevision == revision) {
        retire(g_instances[slot]);
    }
}

/**
 * Validates one mode's source in a probe VM, then installs it and retires that mode's old VMs.
 * Called before ingress starts, from `load_source`.
 * @param modeKey Installed mode the source governs.
 * @param source Lua chunk; an empty or invalid one leaves the mode without a policy.
 * @return True when the source opened and was installed.
 */
bool configure(std::uint32_t modeKey, std::span<const char> source) noexcept {
    policy::install_deferred(&flush, &report_abort);
    const policy::DeferredScope deferred;
    // Probe initialization and destruction do not require either runtime or State lock.
    const std::unique_ptr<vm::Vm> probe(new (std::nothrow) vm::Vm);
    const bool bounded = !source.empty() && source.size() <= vm::kSourceByteCapacity;
    const auto status =
        probe && bounded
            ? vm::open_match_policy(*probe, {g_library.text.data(), g_library.size}, source)
            : vm::OpenStatus::invalidProgram;
    if (status != vm::OpenStatus::ready && probe && bounded) {
        vm::Snapshot snapshot{};
        vm::snapshot(*probe, snapshot);
        report(
            "validate", vm::status_name(status), {.modeKey = modeKey}, snapshot.lastError.data());
    }
    const std::lock_guard lock(g_lock);
    auto* target = source_for(modeKey);
    if (target == nullptr) {
        return false;
    }
    target->size = 0;
    for (auto& instance : g_instances) {
        if (instance.binding.modeKey == modeKey) {
            retire(instance);
        }
    }
    if (status != vm::OpenStatus::ready) {
        return false;
    }
    std::copy(source.begin(), source.end(), target->text.begin());
    target->size = source.size();
    policy::install_releaser(&release);
    policy::install(&evaluate);
    return true;
}

/**
 * Installs the library every mode's program runs first. Called before ingress starts, from
 * `initialize`, ahead of every mode it validates.
 * @param library Lua chunk; an empty or oversized one leaves every mode without a policy.
 * @return True when the library was installed.
 */
bool configure_library(std::span<const char> library) noexcept {
    const std::lock_guard lock(g_lock);
    g_library.size = 0;
    if (library.empty() || library.size() > g_library.text.size()) {
        return false;
    }
    std::copy(library.begin(), library.end(), g_library.text.begin());
    g_library.size = library.size();
    return true;
}

/** Why a policy file could not be read. */
enum class FileStatus : std::uint8_t { ready, missing, unreadable, tooLarge };

[[nodiscard]] std::string_view file_status_name(FileStatus status) noexcept {
    switch (status) {
    case FileStatus::ready:
        return "ready";
    case FileStatus::missing:
        return "missing";
    case FileStatus::tooLarge:
        return "too_large";
    case FileStatus::unreadable:
        break;
    }
    return "unreadable";
}

/** The folder below a `scripts` folder that holds match policies. */
constexpr std::wstring_view kMatchesFolder = L"\\matches\\";
/** Separates a directory from its leaf. */
constexpr std::wstring_view kPathSeparator = L"\\";

/** Reads one whole file into `g_fileBuffer`. */
[[nodiscard]] FileStatus read_file(const core::path::Buffer& path, std::size_t& size) noexcept {
    size = 0;
    HANDLE file = CreateFileW(path.chars.data(),
                              GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_DELETE,
                              nullptr,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND
                   ? FileStatus::missing
                   : FileStatus::unreadable;
    }
    LARGE_INTEGER length{};
    const bool sized = GetFileSizeEx(file, &length) != FALSE;
    if (!sized || length.QuadPart <= 0
        || static_cast<unsigned long long>(length.QuadPart) > g_fileBuffer.size()) {
        CloseHandle(file);
        return sized ? FileStatus::tooLarge : FileStatus::unreadable;
    }
    DWORD read = 0;
    const auto requested = static_cast<DWORD>(length.QuadPart);
    const bool loaded = ReadFile(file, g_fileBuffer.data(), requested, &read, nullptr) != FALSE
                        && read == requested;
    CloseHandle(file);
    if (!loaded) {
        return FileStatus::unreadable;
    }
    size = read;
    return FileStatus::ready;
}

/** One policy file looked up through the loaded mods. */
struct ModLookup final {
    std::wstring_view file;
    FileStatus status{FileStatus::missing};
    std::size_t size{};
};

/**
 * Reads the policy file from the last loaded mod that ships it.
 * @return Always true; the lookup status says what was found.
 */
[[nodiscard]] bool read_mod_policy(void* opaque, const state::mods::View& view) noexcept {
    auto& lookup = *static_cast<ModLookup*>(opaque);
    // A later mod in load order replaces an earlier mod's policy, and every mod replaces the base.
    for (std::size_t index = view.loadedCount; index-- > 0;) {
        const state::mods::Record& record = view.records[index];
        if (!record.hasScripts) {
            continue;
        }
        std::array<wchar_t, state::mods::kFolderCapacity> folder{};
        const std::string_view text = record.folder.view();
        std::copy(text.begin(), text.end(), folder.begin());
        core::path::Buffer path{};
        if (!core::path::append(path, view.modsDirectory)
            || !core::path::append(path, kPathSeparator)
            || !core::path::append(path, {folder.data(), text.size()})
            || !core::path::append(path, kPathSeparator)
            || !core::path::append(path, state::mods::kScriptsFolder)
            || !core::path::append(path, kMatchesFolder)
            || !core::path::append(path, lookup.file)) {
            continue;
        }
        lookup.status = read_file(path, lookup.size);
        if (lookup.status != FileStatus::missing) {
            return true;
        }
    }
    return true;
}

/**
 * Reads one policy file into `g_fileBuffer`, the way mission scripts are found: from the last
 * loaded mod that ships it, then from `scripts\matches` in the Sunrise folder.
 */
[[nodiscard]] FileStatus
read_policy(std::wstring_view file, std::size_t& size, bool& fromMod) noexcept {
    size = 0;
    ModLookup lookup{file};
    // No published mod list means no mods, so a failed visit leaves the Sunrise folder to read.
    fromMod = state::mods::visit(&read_mod_policy, &lookup) && lookup.status != FileStatus::missing;
    if (fromMod) {
        size = lookup.size;
        return lookup.status;
    }
    HMODULE module{};
    core::path::Buffer path{};
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                               | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&read_policy),
                           &module)
            == FALSE
        || !core::path::artifact_directory(module, path)
        || !core::path::append(path, kPathSeparator)
        || !core::path::append(path, state::mods::kScriptsFolder)
        || !core::path::append(path, kMatchesFolder) || !core::path::append(path, file)) {
        return FileStatus::unreadable;
    }
    return read_file(path, size);
}

void load_source(Source& descriptor) noexcept {
    std::size_t size = 0;
    bool fromMod = false;
    const FileStatus status = read_policy(descriptor.file, size, fromMod);
    if (status == FileStatus::ready && configure(descriptor.modeKey, {g_fileBuffer.data(), size})) {
        report("source",
               fromMod ? "loaded_from_mod" : "loaded",
               {.modeKey = descriptor.modeKey},
               descriptor.name);
        return;
    }
    // A mode without a readable, valid policy has no match rules.
    static_cast<void>(configure(descriptor.modeKey, {}));
    report("source",
           status == FileStatus::ready ? "invalid" : file_status_name(status),
           {.modeKey = descriptor.modeKey},
           descriptor.name);
}
} // namespace

void initialize() noexcept {
    shutdown();
    std::size_t size = 0;
    bool fromMod = false;
    const FileStatus library = read_policy(g_library.file, size, fromMod);
    const bool libraryReady =
        library == FileStatus::ready && configure_library({g_fileBuffer.data(), size});
    report("source",
           libraryReady ? (fromMod ? "loaded_from_mod" : "loaded")
                        : (library == FileStatus::ready ? "invalid" : file_status_name(library)),
           {},
           g_library.name);
    for (auto& source : g_sources) {
        if (libraryReady) {
            load_source(source);
        }
    }
    g_fileBuffer.fill({});
}

void shutdown() noexcept {
    policy::install(nullptr);
    policy::install_releaser(nullptr);
    const policy::DeferredScope deferred;
    const std::lock_guard lock(g_lock);
    for (auto& source : g_sources) {
        source.size = 0;
        source.text.fill({});
    }
    g_library.size = 0;
    g_library.text.fill({});
    for (auto& instance : g_instances) {
        retire(instance);
    }
}
} // namespace sunrise::server::activity::match_scripts
