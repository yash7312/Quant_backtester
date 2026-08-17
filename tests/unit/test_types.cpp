#include <gtest/gtest.h>
#include "qback/domain/types.h"

using namespace qback::domain;

TEST(Types, SideSign) {
    EXPECT_EQ(side_sign(Side::Buy), 1);
    EXPECT_EQ(side_sign(Side::Sell), -1);
}

TEST(Types, SideToString) {
    EXPECT_STREQ(to_string(Side::Buy), "Buy");
    EXPECT_STREQ(to_string(Side::Sell), "Sell");
}

TEST(Types, OrderTypeToString) {
    EXPECT_STREQ(to_string(OrderType::Market), "Market");
    EXPECT_STREQ(to_string(OrderType::Limit), "Limit");
}

TEST(Types, OrderStatusToString) {
    EXPECT_STREQ(to_string(OrderStatus::New), "New");
    EXPECT_STREQ(to_string(OrderStatus::Filled), "Filled");
    EXPECT_STREQ(to_string(OrderStatus::PartialFill), "PartialFill");
    EXPECT_STREQ(to_string(OrderStatus::Rejected), "Rejected");
}

TEST(Types, EventTypePriorityOrdering) {
    // Simulation contract §2: CorporateAction < MarketData < Fill < Mark < Signal < Order
    EXPECT_LT(static_cast<uint8_t>(EventType::CorporateAction),
              static_cast<uint8_t>(EventType::MarketData));
    EXPECT_LT(static_cast<uint8_t>(EventType::MarketData),
              static_cast<uint8_t>(EventType::FillEvent));
    EXPECT_LT(static_cast<uint8_t>(EventType::FillEvent),
              static_cast<uint8_t>(EventType::MarkToMarket));
    EXPECT_LT(static_cast<uint8_t>(EventType::MarkToMarket),
              static_cast<uint8_t>(EventType::StrategySignal));
    EXPECT_LT(static_cast<uint8_t>(EventType::StrategySignal),
              static_cast<uint8_t>(EventType::OrderSubmission));
}
