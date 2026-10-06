#include <gtest/gtest.h>

#include "core/EventBus.h"

TEST(EventBus, EventsAreTypedQueuedAndDeliveredOnceToAllListeners) {
    EventBus bus;
    int first = 0, second = 0, other = 0;
    bus.subscribe<InteractionDone>([&](const InteractionDone& e) {
        EXPECT_EQ(e.interactableId, "door");
        ++first;
    });
    bus.subscribe<InteractionDone>([&](const InteractionDone&) { ++second; });
    bus.subscribe<SecurityLooped>([&](const SecurityLooped&) { ++other; });
    bus.publish(InteractionDone{"door"});
    EXPECT_EQ(first, 0);
    bus.dispatch();
    bus.dispatch();
    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 1);
    EXPECT_EQ(other, 0);
}

TEST(EventBus, ListenerPublishedEventsWaitUntilNextTickEvenWithRecursiveDispatch) {
    EventBus bus;
    int done = 0;
    bus.subscribe<InteractionProgress>([&](const InteractionProgress&) {
        bus.publish(InteractionDone{"door"});
        bus.dispatch();
    });
    bus.subscribe<InteractionDone>([&](const InteractionDone&) { ++done; });
    bus.publish(InteractionProgress{"door", 0.5f});
    bus.dispatch();
    EXPECT_EQ(done, 0);
    bus.dispatch();
    EXPECT_EQ(done, 1);
}

TEST(EventBus, EmptyQueueIsSafeAndListenerSubscriptionStartsWithLaterEvents) {
    EventBus bus;
    bus.dispatch();
    int late = 0;
    bool registered = false;
    bus.subscribe<InteractionDone>([&](const InteractionDone&) {
        if (!registered) {
            registered = true;
            bus.subscribe<InteractionDone>([&](const InteractionDone&) { ++late; });
        }
    });
    bus.publish(InteractionDone{"first"});
    bus.dispatch();
    EXPECT_EQ(late, 0);
    bus.publish(InteractionDone{"second"});
    bus.dispatch();
    EXPECT_EQ(late, 1);
}
