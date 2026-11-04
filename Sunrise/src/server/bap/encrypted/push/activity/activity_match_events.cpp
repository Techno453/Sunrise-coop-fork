#include "activity_match_events.h"

#include <algorithm>

#include "../../../../../core/logging/log.h"
#include "../../../../../middleware/bap/activity_message/kill_incident_codec.h"
#include "../../../../../middleware/secure_channel/runtime.h"
#include "../../../../../state/activity/match_state/runtime.h"
#include "activity_notification_frame.h"
namespace sunrise::server::bap::encrypted::push::activity {
namespace match = state::activity::match_state;
namespace incident = middleware::bap::activity_message::incident;
namespace {
void current_queue(Session& session) noexcept {
    if (session.activityMatchEventGeneration != session.activity.bindingGeneration) {
        session.activityMatchEventCount = 0;
        session.activityMatchEventsStaged = 0;
        session.activityMatchNoticesStaged = {};
        session.activityMatchEventGeneration = session.activity.bindingGeneration;
    }
}
} // namespace
void queue_match_event(const ActivityClientBinding& source,
                       std::uint64_t member,
                       std::span<const std::byte> body) noexcept {
    if (!member || body.empty() || body.size() > incident::kMaximumBodyBytes) {
        return;
    }
    for (auto& peer : sessions()) {
        if (!peer.authenticated || !peer.activityMemberKey || peer.activityMemberKey == member
            || peer.activity.role == ActivityClientRole::none
            || peer.activityJoinGeneration != peer.activity.bindingGeneration
            || !state::activity::same_binding(peer.activity.session, source.session)) {
            continue;
        }
        current_queue(peer);
        if (peer.activityMatchEventCount == peer.activityMatchEvents.size()) {
            core::log::write(core::log::Channel::server,
                             core::log::Level::warn,
                             "ev=activity_match stage=event_queue result=full dropped=1");
            continue;
        }
        auto& entry = peer.activityMatchEvents[peer.activityMatchEventCount++];
        entry.size = body.size();
        std::copy(body.begin(), body.end(), entry.body.begin());
    }
}
bool match_events_due(Session& session) noexcept {
    current_queue(session);
    match::NoticeBatch notices{};
    return session.activityMatchEventCount != 0
           || match::pending_notices(
               session.activity.session.sessionId, session.activityMemberKey, notices);
}
bool append_match_events(Session& session,
                         Scratch& scratch,
                         std::array<std::byte, state::kBapNonceSize>& nonce,
                         std::size_t& written) noexcept {
    current_queue(session);
    discard_match_events(session);
    bool appended = false;
    const auto append = [&](std::span<const std::byte> body) noexcept {
        if (!append_notification_frame(scratch,
                                       session.activity.session.sessionId,
                                       incident::kMessageType,
                                       body,
                                       session.sessionKey,
                                       nonce,
                                       scratch.framed,
                                       written)) {
            return false;
        }
        middleware::secure_channel::advance_nonce(nonce);
        appended = true;
        return true;
    };
    for (std::size_t i = 0; i < session.activityMatchEventCount; ++i) {
        const auto& event = session.activityMatchEvents[i];
        if (!append(std::span(event.body).first(event.size))) {
            break;
        }
        ++session.activityMatchEventsStaged;
    }
    match::NoticeBatch notices{};
    match::State state{};
    state::build_data::activity_rules::Rules rules{};
    // Notices are checked first; the match is read for its rules without entering Lua.
    if (match::pending_notices(
            session.activity.session.sessionId, session.activityMemberKey, notices)
        && match::read(session.activity.session.sessionId, state, &rules)
        && rules.incidentTargets != nullptr) {
        session.activityMatchNoticesStaged = notices;
        session.activityMatchNoticesStaged.count = 0;
        for (std::size_t i = 0; i < notices.count; ++i) {
            const auto& notice = notices.notices[i];
            std::array<std::byte, incident::kMaximumBodyBytes> body{};
            std::size_t size{};
            if (!incident::encode_score_notice(*rules.incidentTargets,
                                               notice.target,
                                               notice.sequence,
                                               notice.player,
                                               notice.amount,
                                               body,
                                               size)
                || !append(std::span(body).first(size))) {
                break;
            }
            ++session.activityMatchNoticesStaged.count;
        }
    }
    return appended;
}
void commit_match_events(Session& session) noexcept {
    current_queue(session);
    const auto count = session.activityMatchEventsStaged;
    if (count <= session.activityMatchEventCount) {
        for (std::size_t i = count; i < session.activityMatchEventCount; ++i) {
            session.activityMatchEvents[i - count] = session.activityMatchEvents[i];
        }
        session.activityMatchEventCount -= count;
    }
    match::commit_notices(session.activity.session.sessionId,
                          session.activityMatchNoticesStaged,
                          session.activityMatchNoticesStaged.count);
    discard_match_events(session);
}
void discard_match_events(Session& session) noexcept {
    session.activityMatchEventsStaged = 0;
    session.activityMatchNoticesStaged = {};
}
} // namespace sunrise::server::bap::encrypted::push::activity
