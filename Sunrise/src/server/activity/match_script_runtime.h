#pragma once

namespace sunrise::server::activity::match_scripts {

/**
 * Loads the match policies before activity ingress starts: `common.lua`, then each mode's file,
 * from a loaded mod's `scripts\matches` folder or else from `scripts\matches` in the Sunrise
 * folder. A mode whose file is missing or invalid has no match rules. Restart the server after an
 * edit.
 */
void initialize() noexcept;
/** Stops decisions and releases every per-record VM after ingress stops. */
void shutdown() noexcept;

} // namespace sunrise::server::activity::match_scripts
