#include <gtest/gtest.h>
#include "qback/engine/event_queue.h"

using namespace qback;

TEST(EventQueue, EmptyOnConstruction) {
    engine::EventQueue q;
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size(), 0u);
}

TEST(EventQueue, PushAndPop) {
    engine::EventQueue q;
    auto seq = q.next_sequence();
    q.push(domain::make_market_data_event(100, seq, "SPY", domain::Bar{}));

    EXPECT_FALSE(q.empty());
    EXPECT_EQ(q.size(), 1u);

    auto event = q.pop();
    EXPECT_EQ(event.key.timestamp, 100);
    EXPECT_TRUE(q.empty());
}

TEST(EventQueue, PopsInChronologicalOrder) {
    engine::EventQueue q;
    q.push(domain::make_market_data_event(300, q.next_sequence(), "C", domain::Bar{}));
    q.push(domain::make_market_data_event(100, q.next_sequence(), "A", domain::Bar{}));
    q.push(domain::make_market_data_event(200, q.next_sequence(), "B", domain::Bar{}));

    EXPECT_EQ(q.pop().key.timestamp, 100);
    EXPECT_EQ(q.pop().key.timestamp, 200);
    EXPECT_EQ(q.pop().key.timestamp, 300);
}

TEST(EventQueue, SameTimestampRespectsPriority) {
    engine::EventQueue q;
    // Push in reverse priority order
    q.push(domain::make_order_submission_event(100, q.next_sequence(), 1));
    q.push(domain::make_strategy_signal_event(100, q.next_sequence(), 100));
    q.push(domain::make_market_data_event(100, q.next_sequence(), "SPY", domain::Bar{}));

    auto e1 = q.pop();
    auto e2 = q.pop();
    auto e3 = q.pop();

    // MarketData(1) < StrategySignal(4) < OrderSubmission(5)
    EXPECT_EQ(e1.type, domain::EventType::MarketData);
    EXPECT_EQ(e2.type, domain::EventType::StrategySignal);
    EXPECT_EQ(e3.type, domain::EventType::OrderSubmission);
}

TEST(EventQueue, SamePriorityRespectsSequence) {
    engine::EventQueue q;
    // Two market data events at the same timestamp — sequence breaks tie
    auto seq1 = q.next_sequence();
    auto seq2 = q.next_sequence();
    q.push(domain::make_market_data_event(100, seq2, "QQQ", domain::Bar{}));
    q.push(domain::make_market_data_event(100, seq1, "SPY", domain::Bar{}));

    auto e1 = q.pop();
    auto e2 = q.pop();

    EXPECT_EQ(e1.key.sequence, seq1);  // lower sequence first
    EXPECT_EQ(e2.key.sequence, seq2);
}

TEST(EventQueue, SequenceCounterMonotonicallyIncreases) {
    engine::EventQueue q;
    auto s0 = q.next_sequence();
    auto s1 = q.next_sequence();
    auto s2 = q.next_sequence();

    EXPECT_EQ(s0, 0u);
    EXPECT_EQ(s1, 1u);
    EXPECT_EQ(s2, 2u);
}

TEST(EventQueue, Peek) {
    engine::EventQueue q;
    q.push(domain::make_market_data_event(200, q.next_sequence(), "B", domain::Bar{}));
    q.push(domain::make_market_data_event(100, q.next_sequence(), "A", domain::Bar{}));

    EXPECT_EQ(q.peek().key.timestamp, 100);
    EXPECT_EQ(q.size(), 2u);  // peek doesn't remove
}
