#include <gtest/gtest.h>
#include "qback/engine/clock.h"

using namespace qback::engine;

TEST(Clock, StartsBeforeAnyTimestamp) {
    Clock c;
    EXPECT_FALSE(c.started());
    EXPECT_EQ(c.current(), Clock::BEFORE_START);
}

TEST(Clock, AdvancesForward) {
    Clock c;
    c.advance_to(100);
    EXPECT_TRUE(c.started());
    EXPECT_EQ(c.current(), 100);

    c.advance_to(200);
    EXPECT_EQ(c.current(), 200);
}

TEST(Clock, AdvanceToSameTimeIsAllowed) {
    Clock c;
    c.advance_to(100);
    c.advance_to(100);  // same timestamp — no exception
    EXPECT_EQ(c.current(), 100);
}

TEST(Clock, CannotGoBackwards) {
    Clock c;
    c.advance_to(100);
    EXPECT_THROW(c.advance_to(99), std::logic_error);
}

TEST(Clock, Reset) {
    Clock c;
    c.advance_to(100);
    c.reset();
    EXPECT_FALSE(c.started());
    EXPECT_EQ(c.current(), Clock::BEFORE_START);
}
