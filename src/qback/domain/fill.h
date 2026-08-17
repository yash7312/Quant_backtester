#pragma once

#include <cstdint>
#include <string>

#include "qback/domain/bar.h"
#include "qback/domain/types.h"

namespace qback::domain {

struct Fill {
    uint64_t fill_id;
    uint64_t order_id;
    std::string symbol;
    Side side;
    int64_t quantity;          // shares filled (always positive)
    double price;              // raw execution price before costs
    double commission;         // itemized: broker commission
    double spread_cost;        // itemized: half-spread crossing
    double impact_cost;        // itemized: market impact

    double total_cost() const { return commission + spread_cost + impact_cost; }

    // effective price including all costs (worse for the trader)
    double effective_price() const {
        return price + side_sign(side) * (spread_cost + impact_cost) / quantity;
    }

    // cash impact: negative for buys (cash leaves), positive for sells
    double cash_delta() const {
        double gross = price * quantity;
        double costs = total_cost();
        return side == Side::Buy ? -(gross + costs) : (gross - costs);
    }

    Date fill_date;
};

}  // namespace qback::domain
