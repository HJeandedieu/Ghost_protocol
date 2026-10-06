#include "systems/PagerSystem.h"

#include <algorithm>
#include <cmath>

#include "core/EventBus.h"
#include "systems/InteractionSystem.h"
#include "world/World.h"

PagerSystem::PagerSystem(EventBus& events, const PagerConfig& config, World& world,
                         InteractionSystem& interaction)
    : events_(events), config_(config) {
    pagers_.reserve(world.guards.size());
    for (const auto& guard : world.guards)
        if (guard.hasPager()) pagers_.push_back({guard.id});
    events_.subscribe<GuardTakenDown>([this, &world, &interaction](const auto& event) {
        if (!event.hasPager) return;
        const auto pager = std::find_if(pagers_.begin(), pagers_.end(),
                                        [&](const auto& item) { return item.id == event.guardId; });
        if (pager == pagers_.end() || pager->state != PagerState::Inactive) return;
        const auto body =
            std::find_if(world.guards.begin(), world.guards.end(), [&](const auto& guard) {
                return guard.id == event.guardId && guard.state() == GuardState::Unconscious;
            });
        if (body == world.guards.end()) return;
        pager->state = PagerState::Waiting;
        pager->elapsed = 0;
        interaction.add(
            {"pager:" + pager->id, body->pos, config_.answerHold, "Hold E: answer pager",
             [this, id = pager->id](const auto&) { return state(id) == PagerState::Ringing; },
             [this, id = pager->id](World&) {
                 for (auto& item : pagers_)
                     if (item.id == id && item.state == PagerState::Ringing) {
                         item.state = PagerState::Answered;
                         events_.publish(PagerAnswered{id});
                         break;
                     }
             }});
    });
}

void PagerSystem::update(float dt) {
    if (!std::isfinite(dt) || dt <= 0) return;
    for (auto& pager : pagers_) {
        if (pager.state != PagerState::Waiting && pager.state != PagerState::Ringing) continue;
        pager.elapsed += dt;
        if (pager.state == PagerState::Waiting && pager.elapsed >= config_.ringDelay) {
            pager.state = PagerState::Ringing;
            events_.publish(PagerRang{pager.id});
        }
        if (pager.state == PagerState::Ringing &&
            pager.elapsed >= config_.ringDelay + config_.answerWindow) {
            pager.state = PagerState::Missed;
            events_.publish(PagerMissed{pager.id});
        }
    }
}

PagerState PagerSystem::state(const std::string& bodyId) const {
    for (const auto& pager : pagers_)
        if (pager.id == bodyId) return pager.state;
    return PagerState::Inactive;
}

float PagerSystem::remaining(const std::string& bodyId) const {
    for (const auto& pager : pagers_)
        if (pager.id == bodyId && pager.state == PagerState::Ringing)
            return std::max(0.0, config_.ringDelay + config_.answerWindow - pager.elapsed);
    return 0;
}
