#pragma once

#include <cmath>
#include <string>

namespace qback::domain {

struct Position {
    std::string symbol;
    int64_t quantity = 0;       // positive = long, negative = short
    double avg_cost = 0.0;      // volume-weighted average entry price
    double mark_price = 0.0;    // most recent close used for marking
    double realized_pnl = 0.0;  // cumulative realized PnL for this symbol

    double market_value() const { return quantity * mark_price; }

    double unrealized_pnl() const {
        return quantity * (mark_price - avg_cost);
    }

    bool is_flat() const { return quantity == 0; }

    // Apply a fill: update quantity, avg_cost, and realized_pnl.
    // buy_quantity is signed: positive for buys, negative for sells.
    void apply(int64_t signed_qty, double fill_price) {
        if (signed_qty == 0) return;

        bool increasing = (quantity >= 0 && signed_qty > 0)
                       || (quantity <= 0 && signed_qty < 0);

        if (increasing) {
            // Adding to position: update average cost
            double old_value = quantity * avg_cost;
            double new_value = signed_qty * fill_price;
            quantity += signed_qty;
            avg_cost = (quantity != 0) ? (old_value + new_value) / quantity : 0.0;
        } else {
            // Reducing or flipping position: realize PnL on closed portion
            int64_t closed = std::min(std::abs(signed_qty), std::abs(quantity));
            double pnl_per_share = (quantity > 0)
                ? (fill_price - avg_cost)
                : (avg_cost - fill_price);
            realized_pnl += closed * pnl_per_share;

            quantity += signed_qty;

            // If we flipped sides, the remaining quantity has the new fill price as its cost
            if ((quantity > 0 && signed_qty > 0) || (quantity < 0 && signed_qty < 0)) {
                // shouldn't happen in this branch, but be safe
            } else if (quantity != 0 && std::abs(signed_qty) > closed) {
                avg_cost = fill_price;
            }
            // If flat, leave avg_cost as-is (doesn't matter)
        }
    }
};

}  // namespace qback::domain
