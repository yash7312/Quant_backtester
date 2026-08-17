#pragma once

#include <cstdint>
#include <compare>
#include <variant>
#include <string>

#include "qback/domain/bar.h"
#include "qback/domain/types.h"

namespace qback::domain {

struct EventKey {
    int32_t timestamp;     // days since epoch (same as Date.days_since_epoch)
    uint8_t priority;      // EventType's underlying value — lower fires first
    uint64_t sequence;     // monotonic tie-breaker — lower fires first

    auto operator<=>(const EventKey&) const = default;
};

struct MarketDataPayload {
    std::string symbol;
    Bar bar;
};

struct StrategySignalPayload {
    int32_t as_of_date;  // the bar date this signal was computed from
};

struct OrderSubmissionPayload {
    uint64_t order_id;
};

struct FillPayload {
    uint64_t fill_id;
};

struct MarkToMarketPayload {};

struct CorporateActionPayload {
    std::string symbol;
};

using EventPayload = std::variant<
    MarketDataPayload,
    StrategySignalPayload,
    OrderSubmissionPayload,
    FillPayload,
    MarkToMarketPayload,
    CorporateActionPayload
>;

struct Event {
    EventKey key;
    EventType type;
    EventPayload payload;

    bool operator>(const Event& other) const {
        return key > other.key;
    }
};

inline Event make_market_data_event(int32_t date, uint64_t seq,
                                    const std::string& symbol, const Bar& bar) {
    return {
        .key = {date, static_cast<uint8_t>(EventType::MarketData), seq},
        .type = EventType::MarketData,
        .payload = MarketDataPayload{symbol, bar},
    };
}

inline Event make_strategy_signal_event(int32_t date, uint64_t seq,
                                        int32_t as_of_date) {
    return {
        .key = {date, static_cast<uint8_t>(EventType::StrategySignal), seq},
        .type = EventType::StrategySignal,
        .payload = StrategySignalPayload{as_of_date},
    };
}

inline Event make_order_submission_event(int32_t date, uint64_t seq,
                                         uint64_t order_id) {
    return {
        .key = {date, static_cast<uint8_t>(EventType::OrderSubmission), seq},
        .type = EventType::OrderSubmission,
        .payload = OrderSubmissionPayload{order_id},
    };
}

}  // namespace qback::domain
