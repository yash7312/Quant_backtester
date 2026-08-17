#include <gtest/gtest.h>

#include "qback/domain/order.h"
#include "qback/domain/fill.h"

using namespace qback::domain;

// Simulation contract §3: eligible_date > submitted_date
TEST(Order, EligibleDateAfterSubmittedDate) {
    Order o{
        .order_id = 1,
        .symbol = "SPY",
        .side = Side::Buy,
        .type = OrderType::Market,
        .quantity = 100,
        .limit_price = std::nullopt,
        .tif = TIF::Day,
        .status = OrderStatus::New,
        .submitted_date = Date{19000},   // some arbitrary day
        .eligible_date = Date{19001},    // next day
    };

    EXPECT_GT(o.eligible_date.days_since_epoch, o.submitted_date.days_since_epoch);
}

TEST(Order, RemainingQuantity) {
    Order o{
        .order_id = 1,
        .symbol = "SPY",
        .side = Side::Buy,
        .type = OrderType::Market,
        .quantity = 100,
        .tif = TIF::Day,
        .status = OrderStatus::Active,
        .submitted_date = Date{19000},
        .eligible_date = Date{19001},
        .filled_quantity = 30,
    };

    EXPECT_EQ(o.remaining(), 70);
}

TEST(Order, TerminalStates) {
    Order o{.order_id = 1, .symbol = "SPY", .side = Side::Buy, .type = OrderType::Market,
            .quantity = 100, .tif = TIF::Day, .status = OrderStatus::New,
            .submitted_date = Date{0}, .eligible_date = Date{1}};

    o.status = OrderStatus::New;
    EXPECT_FALSE(o.is_terminal());

    o.status = OrderStatus::Active;
    EXPECT_FALSE(o.is_terminal());

    o.status = OrderStatus::PartialFill;
    EXPECT_FALSE(o.is_terminal());

    o.status = OrderStatus::Filled;
    EXPECT_TRUE(o.is_terminal());

    o.status = OrderStatus::Cancelled;
    EXPECT_TRUE(o.is_terminal());

    o.status = OrderStatus::Rejected;
    EXPECT_TRUE(o.is_terminal());
}

// Simulation contract §8: costs are non-negative
TEST(Fill, CostsNonNegative) {
    Fill f{
        .fill_id = 1, .order_id = 1, .symbol = "SPY", .side = Side::Buy,
        .quantity = 100, .price = 300.0,
        .commission = 0.50, .spread_cost = 1.50, .impact_cost = 0.75,
        .fill_date = Date{19001},
    };

    EXPECT_GE(f.commission, 0.0);
    EXPECT_GE(f.spread_cost, 0.0);
    EXPECT_GE(f.impact_cost, 0.0);
    EXPECT_GE(f.total_cost(), 0.0);
}

TEST(Fill, CashDeltaBuy) {
    Fill f{
        .fill_id = 1, .order_id = 1, .symbol = "SPY", .side = Side::Buy,
        .quantity = 100, .price = 300.0,
        .commission = 0.50, .spread_cost = 1.50, .impact_cost = 0.0,
        .fill_date = Date{19001},
    };

    // Buy: cash decreases by (price × qty + costs)
    EXPECT_DOUBLE_EQ(f.cash_delta(), -(300.0 * 100 + 2.0));
}

TEST(Fill, CashDeltaSell) {
    Fill f{
        .fill_id = 1, .order_id = 1, .symbol = "SPY", .side = Side::Sell,
        .quantity = 100, .price = 310.0,
        .commission = 0.50, .spread_cost = 1.50, .impact_cost = 0.0,
        .fill_date = Date{19001},
    };

    // Sell: cash increases by (price × qty - costs)
    EXPECT_DOUBLE_EQ(f.cash_delta(), 310.0 * 100 - 2.0);
}
