#include <gtest/gtest.h>
#include <algorithm>
#include <vector>

#include "qback/domain/event.h"

using namespace qback::domain;

TEST(EventKey, SameTimestampPriorityOrdering) {
    // Simulation contract §2: lower priority number fires first
    EventKey market_data{100, static_cast<uint8_t>(EventType::MarketData), 0};
    EventKey strategy{100, static_cast<uint8_t>(EventType::StrategySignal), 0};
    EventKey order_sub{100, static_cast<uint8_t>(EventType::OrderSubmission), 0};

    EXPECT_LT(market_data, strategy);
    EXPECT_LT(strategy, order_sub);
    EXPECT_LT(market_data, order_sub);
}

TEST(EventKey, TimestampTakesPrecedence) {
    // Earlier timestamp always wins, regardless of priority
    EventKey earlier_low_prio{99, static_cast<uint8_t>(EventType::OrderSubmission), 0};
    EventKey later_high_prio{100, static_cast<uint8_t>(EventType::CorporateAction), 0};

    EXPECT_LT(earlier_low_prio, later_high_prio);
}

TEST(EventKey, SequenceBreaksTies) {
    EventKey first{100, static_cast<uint8_t>(EventType::MarketData), 0};
    EventKey second{100, static_cast<uint8_t>(EventType::MarketData), 1};

    EXPECT_LT(first, second);
}

TEST(EventKey, SortingProducesCorrectOrder) {
    std::vector<EventKey> keys = {
        {100, static_cast<uint8_t>(EventType::OrderSubmission), 0},
        {100, static_cast<uint8_t>(EventType::MarketData), 1},
        {99,  static_cast<uint8_t>(EventType::StrategySignal), 0},
        {100, static_cast<uint8_t>(EventType::MarketData), 0},
        {100, static_cast<uint8_t>(EventType::StrategySignal), 0},
    };

    std::sort(keys.begin(), keys.end());

    // Day 99 first, then day 100 in priority order, then by sequence
    EXPECT_EQ(keys[0].timestamp, 99);
    EXPECT_EQ(keys[1].priority, static_cast<uint8_t>(EventType::MarketData));
    EXPECT_EQ(keys[1].sequence, 0);
    EXPECT_EQ(keys[2].priority, static_cast<uint8_t>(EventType::MarketData));
    EXPECT_EQ(keys[2].sequence, 1);
    EXPECT_EQ(keys[3].priority, static_cast<uint8_t>(EventType::StrategySignal));
    EXPECT_EQ(keys[4].priority, static_cast<uint8_t>(EventType::OrderSubmission));
}

// Simulation contract §3: signal from bar t cannot fill on bar t
TEST(Event, SignalFiresBeforeOrderOnSameBar) {
    Event signal = make_strategy_signal_event(100, 0, 100);
    Event order = make_order_submission_event(100, 0, 1);

    // Strategy signal has lower priority number → fires first
    EXPECT_LT(signal.key, order.key);
}

TEST(Event, MarketDataFiresBeforeSignalOnSameBar) {
    Event md = make_market_data_event(100, 0, "SPY", Bar{});
    Event signal = make_strategy_signal_event(100, 1, 100);

    EXPECT_LT(md.key, signal.key);
}
