#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "qback/domain/bar.h"
#include "qback/domain/types.h"

namespace qback::domain {

struct Order {
    uint64_t order_id;
    std::string symbol;
    Side side;
    OrderType type;
    int64_t quantity;                       // always positive; side indicates direction
    std::optional<double> limit_price;      // set only for Limit orders
    TIF tif;
    OrderStatus status;

    Date submitted_date;   // bar date when the order was created
    Date eligible_date;    // earliest bar date the order can fill against (submitted + 1)

    int64_t filled_quantity = 0;            // cumulative across partial fills

    int64_t remaining() const { return quantity - filled_quantity; }
    bool is_terminal() const {
        return status == OrderStatus::Filled
            || status == OrderStatus::Cancelled
            || status == OrderStatus::Rejected;
    }
};

}  // namespace qback::domain
