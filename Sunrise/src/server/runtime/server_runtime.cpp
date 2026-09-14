#include "server_runtime.h"

#include "../../client/network/consumer.h"
#include "../../core/logging/log.h"
#include "../../core/settings/settings.h"
#include "../activity/host_runtime.h"
#include "../activity/mission/mission_script_runtime.h"
#include "../bap/runtime.h"
#include "../gameplay/gameplay_runtime.h"
#include "../http/server_http.h"
#include "../transport/bap_listener.h"
#include "../ui/runtime/server_ui_module_runtime.h"

namespace sunrise::server {
namespace {
bool gameplayStarted{};
void stop_gameplay() noexcept {
    if (!gameplayStarted) {
        return;
    }
    gameplayStarted = false;
    gameplay::shutdown();
}
} // namespace

/** Registers Server consumers with the Client networking boundary. */
bool initialize() noexcept {
    activity::host::reset();
    activity::mission::initialize();
    if (!client::network::register_http_consumer(&http::consume)) {
        activity::mission::shutdown();
        return false;
    }
    if (client::network::register_bap_consumer(&bap::consume)) {
        // HTTP and UI remain useful when the local BAP port is already owned.
        if (!transport::initialize()) {
            core::log::write(core::log::Channel::server,
                             core::log::Level::warn,
                             "ev=transport stage=listen result=fail");
            if (core::settings::hosts_session() || core::settings::get().server.upstream.enabled) {
                client::network::unregister_bap_consumer(&bap::consume);
                client::network::unregister_http_consumer(&http::consume);
                activity::mission::shutdown();
                return false;
            }
        }
        // The gameplay endpoint must bind before any descriptor advertises it.
        gameplayStarted = !core::settings::get().server.upstream.enabled && gameplay::initialize();
        if (!core::settings::get().server.upstream.enabled && !gameplayStarted) {
            core::log::write(core::log::Channel::server,
                             core::log::Level::warn,
                             "ev=gameplay stage=init result=fail");
            if (core::settings::hosts_session()) {
                transport::shutdown();
                client::network::unregister_bap_consumer(&bap::consume);
                client::network::unregister_http_consumer(&http::consume);
                activity::mission::shutdown();
                return false;
            }
        }
        if (ui::runtime::initialize()) {
            return true;
        }
        stop_gameplay();
        transport::shutdown();
        client::network::unregister_bap_consumer(&bap::consume);
    }
    // BAP registration failure rolls back the earlier HTTP registration.
    client::network::unregister_http_consumer(&http::consume);
    activity::mission::shutdown();
    return false;
}

/** Runs one bounded server service slice. @param now Monotonic tick count. */
void service(std::uint64_t now) noexcept {
    bap::service(now);
    transport::service(now);
    activity::host::service(now);
    activity::mission::service(now);
    if (gameplayStarted) {
        gameplay::service(now);
    }
}

/** Unregisters Server consumers in reverse registration order. */
void shutdown() noexcept {
    ui::runtime::shutdown();
    stop_gameplay();
    transport::shutdown();
    client::network::unregister_bap_consumer(&bap::consume);
    client::network::unregister_http_consumer(&http::consume);
    bap::shutdown();
    activity::mission::shutdown();
    activity::host::reset();
}

} // namespace sunrise::server
