#pragma once

namespace sunrise::server::activity::match_scripts {

/** Loads the bounded bundled policies before activity ingress starts. */
void initialize() noexcept;
/** Stops decisions and releases every per-record VM after ingress stops. */
void shutdown() noexcept;

} // namespace sunrise::server::activity::match_scripts
