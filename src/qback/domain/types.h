#pragma once

#include <cstdint>
#include <string>

namespace qback::domain {

enum class Side : uint8_t { Buy, Sell };

enum class OrderType : uint8_t { Market, Limit };

enum class OrderStatus : uint8_t {
    New,
    Submitted,
    Active,
    Filled,
    PartialFill,
    Cancelled,
    Rejected,
};

enum class TIF : uint8_t { Day, GTC };

enum class EventType : uint8_t {
    CorporateAction = 0,
    MarketData       = 1,
    FillEvent        = 2,
    MarkToMarket     = 3,
    StrategySignal   = 4,
    OrderSubmission  = 5,
};

inline const char* to_string(Side s) {
    switch (s) {
        case Side::Buy:  return "Buy";
        case Side::Sell: return "Sell";
    }
    return "Unknown";
}

inline const char* to_string(OrderType t) {
    switch (t) {
        case OrderType::Market: return "Market";
        case OrderType::Limit:  return "Limit";
    }
    return "Unknown";
}

inline const char* to_string(OrderStatus s) {
    switch (s) {
        case OrderStatus::New:         return "New";
        case OrderStatus::Submitted:   return "Submitted";
        case OrderStatus::Active:      return "Active";
        case OrderStatus::Filled:      return "Filled";
        case OrderStatus::PartialFill: return "PartialFill";
        case OrderStatus::Cancelled:   return "Cancelled";
        case OrderStatus::Rejected:    return "Rejected";
    }
    return "Unknown";
}

inline const char* to_string(TIF t) {
    switch (t) {
        case TIF::Day: return "Day";
        case TIF::GTC: return "GTC";
    }
    return "Unknown";
}

inline const char* to_string(EventType e) {
    switch (e) {
        case EventType::CorporateAction: return "CorporateAction";
        case EventType::MarketData:      return "MarketData";
        case EventType::FillEvent:       return "FillEvent";
        case EventType::MarkToMarket:    return "MarkToMarket";
        case EventType::StrategySignal:  return "StrategySignal";
        case EventType::OrderSubmission: return "OrderSubmission";
    }
    return "Unknown";
}

inline int side_sign(Side s) {
    return s == Side::Buy ? 1 : -1;
}

}  // namespace qback::domain
