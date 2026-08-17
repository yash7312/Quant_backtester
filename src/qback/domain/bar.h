#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace qback::domain {

struct Date {
    int32_t days_since_epoch;  // Arrow date32: days since 1970-01-01

    int year() const;
    int month() const;
    int day() const;
    std::string to_string() const;
};

struct Bar {
    Date date;
    double open;
    double high;
    double low;
    double close;
    int64_t volume;
    double adjusted_close;
};

struct Instrument {
    std::string symbol;
    std::vector<Bar> bars;

    std::size_t size() const { return bars.size(); }
    bool empty() const { return bars.empty(); }
    const Bar& front() const { return bars.front(); }
    const Bar& back() const { return bars.back(); }
};

}  // namespace qback::domain
