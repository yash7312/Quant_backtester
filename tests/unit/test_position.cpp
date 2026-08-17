#include <gtest/gtest.h>
#include <cmath>

#include "qback/domain/position.h"

using namespace qback::domain;

TEST(Position, StartsFlat) {
    Position p{.symbol = "SPY"};
    EXPECT_TRUE(p.is_flat());
    EXPECT_EQ(p.quantity, 0);
    EXPECT_DOUBLE_EQ(p.realized_pnl, 0.0);
}

TEST(Position, BuyIncreasesPosition) {
    Position p{.symbol = "SPY"};
    p.apply(100, 300.0);  // buy 100 @ $300

    EXPECT_EQ(p.quantity, 100);
    EXPECT_DOUBLE_EQ(p.avg_cost, 300.0);
    EXPECT_DOUBLE_EQ(p.realized_pnl, 0.0);
}

TEST(Position, BuyTwiceBlendsCost) {
    Position p{.symbol = "SPY"};
    p.apply(100, 300.0);  // buy 100 @ $300
    p.apply(100, 310.0);  // buy 100 @ $310

    EXPECT_EQ(p.quantity, 200);
    EXPECT_DOUBLE_EQ(p.avg_cost, 305.0);  // (100*300 + 100*310) / 200
}

TEST(Position, SellRealizesProfit) {
    Position p{.symbol = "SPY"};
    p.apply(100, 300.0);   // buy 100 @ $300
    p.apply(-50, 320.0);   // sell 50 @ $320

    EXPECT_EQ(p.quantity, 50);
    EXPECT_DOUBLE_EQ(p.avg_cost, 300.0);
    EXPECT_DOUBLE_EQ(p.realized_pnl, 50.0 * 20.0);  // $1000 profit
}

TEST(Position, SellRealizesLoss) {
    Position p{.symbol = "SPY"};
    p.apply(100, 300.0);   // buy 100 @ $300
    p.apply(-100, 280.0);  // sell 100 @ $280

    EXPECT_TRUE(p.is_flat());
    EXPECT_DOUBLE_EQ(p.realized_pnl, -2000.0);  // $20 loss × 100 shares
}

TEST(Position, FullCloseAndReopen) {
    Position p{.symbol = "SPY"};
    p.apply(100, 300.0);
    p.apply(-100, 310.0);  // close: realize $1000

    EXPECT_TRUE(p.is_flat());
    EXPECT_DOUBLE_EQ(p.realized_pnl, 1000.0);

    p.apply(50, 320.0);    // reopen
    EXPECT_EQ(p.quantity, 50);
    EXPECT_DOUBLE_EQ(p.avg_cost, 320.0);
    EXPECT_DOUBLE_EQ(p.realized_pnl, 1000.0);  // previous realized preserved
}

TEST(Position, ShortPosition) {
    Position p{.symbol = "SPY"};
    p.apply(-100, 300.0);  // short 100 @ $300

    EXPECT_EQ(p.quantity, -100);
    EXPECT_DOUBLE_EQ(p.avg_cost, 300.0);
}

TEST(Position, ShortCoverRealizesProfit) {
    Position p{.symbol = "SPY"};
    p.apply(-100, 300.0);  // short 100 @ $300
    p.apply(100, 280.0);   // cover  100 @ $280

    EXPECT_TRUE(p.is_flat());
    EXPECT_DOUBLE_EQ(p.realized_pnl, 2000.0);  // shorted at 300, covered at 280 → $20/share profit
}

TEST(Position, ShortCoverRealizesLoss) {
    Position p{.symbol = "SPY"};
    p.apply(-100, 300.0);
    p.apply(100, 320.0);   // cover at higher price → loss

    EXPECT_TRUE(p.is_flat());
    EXPECT_DOUBLE_EQ(p.realized_pnl, -2000.0);
}

TEST(Position, UnrealizedPnl) {
    Position p{.symbol = "SPY"};
    p.apply(100, 300.0);
    p.mark_price = 310.0;

    EXPECT_DOUBLE_EQ(p.unrealized_pnl(), 1000.0);  // 100 × ($310 - $300)
    EXPECT_DOUBLE_EQ(p.market_value(), 31000.0);    // 100 × $310
}

TEST(Position, UnrealizedPnlShort) {
    Position p{.symbol = "SPY"};
    p.apply(-100, 300.0);
    p.mark_price = 290.0;

    EXPECT_DOUBLE_EQ(p.unrealized_pnl(), 1000.0);  // -100 × ($290 - $300) = $1000
    EXPECT_DOUBLE_EQ(p.market_value(), -29000.0);   // -100 × $290
}
