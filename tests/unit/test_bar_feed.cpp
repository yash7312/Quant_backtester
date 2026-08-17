#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "qback/domain/bar.h"
#include "qback/domain/event.h"
#include "qback/engine/bar_feed.h"
#include "qback/engine/event_queue.h"

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

TEST(BarFeed, LoadsSingleInstrument) {
    auto inst = make_instrument("SPY", {100, 101, 102});
    engine::EventQueue q;
    engine::load_bars_into_queue({inst}, q);

    EXPECT_EQ(q.size(), 3u);

    auto e1 = q.pop();
    EXPECT_EQ(e1.key.timestamp, 100);
    EXPECT_EQ(e1.type, domain::EventType::MarketData);

    auto e2 = q.pop();
    EXPECT_EQ(e2.key.timestamp, 101);

    auto e3 = q.pop();
    EXPECT_EQ(e3.key.timestamp, 102);
}

TEST(BarFeed, MultipleInstrumentsInterleavedChronologically) {
    auto spy = make_instrument("SPY", {100, 101});
    auto qqq = make_instrument("QQQ", {100, 101});

    engine::EventQueue q;
    engine::load_bars_into_queue({spy, qqq}, q);

    EXPECT_EQ(q.size(), 4u);

    // Day 100: QQQ before SPY (alphabetical via sequence assignment)
    auto e1 = q.pop();
    EXPECT_EQ(e1.key.timestamp, 100);
    EXPECT_EQ(std::get<domain::MarketDataPayload>(e1.payload).symbol, "QQQ");

    auto e2 = q.pop();
    EXPECT_EQ(e2.key.timestamp, 100);
    EXPECT_EQ(std::get<domain::MarketDataPayload>(e2.payload).symbol, "SPY");

    // Day 101: QQQ before SPY
    auto e3 = q.pop();
    EXPECT_EQ(e3.key.timestamp, 101);
    EXPECT_EQ(std::get<domain::MarketDataPayload>(e3.payload).symbol, "QQQ");

    auto e4 = q.pop();
    EXPECT_EQ(e4.key.timestamp, 101);
    EXPECT_EQ(std::get<domain::MarketDataPayload>(e4.payload).symbol, "SPY");
}

TEST(BarFeed, PreservesBarData) {
    domain::Instrument inst;
    inst.symbol = "GLD";
    domain::Bar bar{};
    bar.date = {200};
    bar.open = 150.5;
    bar.high = 155.0;
    bar.low = 149.0;
    bar.close = 153.0;
    bar.volume = 2000000;
    bar.adjusted_close = 153.0;
    inst.bars.push_back(bar);

    engine::EventQueue q;
    engine::load_bars_into_queue({inst}, q);

    auto event = q.pop();
    const auto& payload = std::get<domain::MarketDataPayload>(event.payload);
    EXPECT_EQ(payload.symbol, "GLD");
    EXPECT_DOUBLE_EQ(payload.bar.open, 150.5);
    EXPECT_DOUBLE_EQ(payload.bar.high, 155.0);
    EXPECT_DOUBLE_EQ(payload.bar.low, 149.0);
    EXPECT_DOUBLE_EQ(payload.bar.close, 153.0);
    EXPECT_EQ(payload.bar.volume, 2000000);
}

TEST(BarFeed, EmptyInstrumentsProduceNoEvents) {
    engine::EventQueue q;
    engine::load_bars_into_queue({}, q);
    EXPECT_TRUE(q.empty());
}

// Determinism: loading the same data produces identical event sequence
TEST(BarFeed, DeterministicOrdering) {
    auto spy = make_instrument("SPY", {100, 101, 102});
    auto qqq = make_instrument("QQQ", {100, 101, 102});
    auto gld = make_instrument("GLD", {100, 101, 102});

    auto load_and_drain = [&]() {
        engine::EventQueue q;
        engine::load_bars_into_queue({spy, qqq, gld}, q);
        std::vector<std::pair<int32_t, std::string>> sequence;
        while (!q.empty()) {
            auto e = q.pop();
            const auto& p = std::get<domain::MarketDataPayload>(e.payload);
            sequence.emplace_back(e.key.timestamp, p.symbol);
        }
        return sequence;
    };

    auto run1 = load_and_drain();
    auto run2 = load_and_drain();

    ASSERT_EQ(run1.size(), run2.size());
    for (size_t i = 0; i < run1.size(); ++i) {
        EXPECT_EQ(run1[i].first, run2[i].first);
        EXPECT_EQ(run1[i].second, run2[i].second);
    }
}
