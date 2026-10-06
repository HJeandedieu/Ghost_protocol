#pragma once

#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "core/Events.h"

class EventBus {
   public:
    EventBus() {
        pending_.reserve(128);
        delivering_.reserve(128);
    }
    template <class E>
    void subscribe(std::function<void(const E&)> fn) {
        std::get<std::vector<std::function<void(const E&)>>>(listeners_).push_back(std::move(fn));
    }
    template <class E>
    void publish(const E& event) {
        pending_.emplace_back(event);
    }
    void dispatch() {
        if (dispatching_) return;
        dispatching_ = true;
        pending_.swap(delivering_);
        for (const auto& event : delivering_) {
            std::visit(
                [this](const auto& value) {
                    using E = std::decay_t<decltype(value)>;
                    auto& listeners =
                        std::get<std::vector<std::function<void(const E&)>>>(listeners_);
                    const auto count = listeners.size();
                    for (std::size_t i = 0; i < count; ++i) {
                        // A callback can subscribe without invalidating the function being invoked.
                        auto callback = listeners[i];
                        callback(value);
                    }
                },
                event);
        }
        delivering_.clear();
        dispatching_ = false;
    }

   private:
    using Event = std::variant<NoiseEmitted, GuardSuspicious, GuardSpotted, CallInStarted,
                               InteractionProgress, InteractionDone, SecurityLooped>;
    std::vector<Event> pending_, delivering_;
    std::tuple<std::vector<std::function<void(const NoiseEmitted&)>>,
               std::vector<std::function<void(const GuardSuspicious&)>>,
               std::vector<std::function<void(const GuardSpotted&)>>,
               std::vector<std::function<void(const CallInStarted&)>>,
               std::vector<std::function<void(const InteractionProgress&)>>,
               std::vector<std::function<void(const InteractionDone&)>>,
               std::vector<std::function<void(const SecurityLooped&)>>>
        listeners_;
    bool dispatching_ = false;
};
