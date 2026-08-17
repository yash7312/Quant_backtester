#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "qback/domain/event.h"
#include "qback/engine/engine.h"
#include "qback/engine/bar_feed.h"

using namespace qback;

namespace {

domain::Instrument make_instrument(const std::string& symbol,
                                    std::vector<int32_t> dates) {
    domain::Instrument inst;
    inst.symbol = symbol;
    for (auto d : dates) {
        domain::Bar bar{};
        bar.date = {d};
        bar.open = 100.0;
        bar.high = 105.0;
        bar.low = 95.0;
        bar.close = 102.0;
        bar.volume = 1000000;
        bar.adjusted_close = 102.0;
        inst.bars.push_back(bar);
    }
    return inst;
}

}  // namespace

TEST(Engine, RunsEmptyQueue) {
    engine::Engine eng;
    auto stats = eng.run();
    EXPECT_EQ(stats.events_processed, 0u);
}

TEST(Engine, DispatchesEventsInOrder) {
    engine::Engine eng;

    std::vector<std::pair<int32_t, std::string>> dispatched;

    eng.set_handler(domain::EventType::MarketData, [&](const domain::Event& e) {
        const auto& p = std::get<domain::MarketDataPayload>(e.payload);
        dispatched.emplace_back(e.key.timestamp, p.symbol);
    });

    auto spy = make_instrument("SPY", {100, 101});
    auto qqq = make_instrument("QQQ", {100, 101});
    engine::load_bars_into_queue({spy, qqq}, eng.queue());

    auto stats = eng.run();

    EXPECT_EQ(stats.events_processed, 4u);
    EXPECT_EQ(stats.first_timestamp, 100);
    EXPECT_EQ(stats.last_timestamp, 101);

    // Verify chronological then alphabetical order
    ASSERT_EQ(dispatched.size(), 4u);
    EXPECT_EQ(dispatched[0], std::make_pair(100, std::string("QQQ")));
    EXPECT_EQ(dispatched[1], std::make_pair(100, std::string("SPY")));
    EXPECT_EQ(dispatched[2], std::make_pair(101, std::string("QQQ")));
    EXPECT_EQ(dispatched[3], std::make_pair(101, std::string("SPY")));
}

TEST(Engine, ClockAdvancesMonotonically) {
    engine::Engine eng;

    std::vector<int32_t> clock_snapshots;

    eng.set_handler(domain::EventType::MarketData, [&](const domain::Event&) {
        clock_snapshots.push_back(eng.clock().current());
    });

    auto spy = make_instrument("SPY", {100, 200, 300});
    engine::load_bars_into_queue({spy}, eng.queue());
    eng.run();

    ASSERT_EQ(clock_snapshots.size(), 3u);
    EXPECT_EQ(clock_snapshots[0], 100);
    EXPECT_EQ(clock_snapshots[1], 200);
    EXPECT_EQ(clock_snapshots[2], 300);
}

TEST(Engine, HandlerCanEnqueueNewEvents) {
    engine::Engine eng;

    int signal_count = 0;
    int md_count = 0;

    eng.set_handler(domain::EventType::MarketData, [&](const domain::Event& e) {
        md_count++;
        // When we see market data, enqueue a strategy signal for the same timestamp
        auto seq = eng.next_sequence();
        eng.push_event(domain::make_strategy_signal_event(
            e.key.timestamp, seq, e.key.timestamp));
    });

    eng.set_handler(domain::EventType::StrategySignal, [&](const domain::Event&) {
        signal_count++;
    });

    auto spy = make_instrument("SPY", {100, 101});
    engine::load_bars_into_queue({spy}, eng.queue());

    auto stats = eng.run();

    EXPECT_EQ(md_count, 2);
    EXPECT_EQ(signal_count, 2);
    EXPECT_EQ(stats.events_processed, 4u);  // 2 MD + 2 signals
}

// Simulation contract §2, §3: events enqueued during dispatch respect priority
TEST(Engine, DynamicEventsRespectPriority) {
    engine::Engine eng;

    std::vector<domain::EventType> dispatch_order;

    eng.set_handler(domain::EventType::MarketData, [&](const domain::Event& e) {
        dispatch_order.push_back(domain::EventType::MarketData);
        // Enqueue both signal and order events for same timestamp
        eng.push_event(domain::make_order_submission_event(
            e.key.timestamp, eng.next_sequence(), 1));
        eng.push_event(domain::make_strategy_signal_event(
            e.key.timestamp, eng.next_sequence(), e.key.timestamp));
    });

    eng.set_handler(domain::EventType::StrategySignal, [&](const domain::Event& e) {
        dispatch_order.push_back(domain::EventType::StrategySignal);
    });

    eng.set_handler(domain::EventType::OrderSubmission, [&](const domain::Event& e) {
        dispatch_order.push_back(domain::EventType::OrderSubmission);
    });

    auto spy = make_instrument("SPY", {100});
    engine::load_bars_into_queue({spy}, eng.queue());
    eng.run();

    // MarketData(1) fires, enqueues Signal(4) and Order(5)
    // Signal(4) fires next, then Order(5)
    ASSERT_EQ(dispatch_order.size(), 3u);
    EXPECT_EQ(dispatch_order[0], domain::EventType::MarketData);
    EXPECT_EQ(dispatch_order[1], domain::EventType::StrategySignal);
    EXPECT_EQ(dispatch_order[2], domain::EventType::OrderSubmission);
}

TEST(Engine, UnhandledEventTypesAreSkipped) {
    engine::Engine eng;

    int md_count = 0;
    eng.set_handler(domain::EventType::MarketData, [&](const domain::Event&) {
        md_count++;
    });

    // Push a signal event with no handler registered
    eng.push_event(domain::make_strategy_signal_event(100, eng.next_sequence(), 100));
    eng.push_event(domain::make_market_data_event(100, eng.next_sequence(), "SPY", domain::Bar{}));

    auto stats = eng.run();

    EXPECT_EQ(stats.events_processed, 2u);  // both processed (iterated)
    EXPECT_EQ(md_count, 1);  // only MD handler fired
}
