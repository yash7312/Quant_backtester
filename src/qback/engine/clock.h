#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace qback::engine {

class Clock {
public:
    static constexpr int32_t BEFORE_START = std::numeric_limits<int32_t>::min();

    int32_t current() const { return current_time_; }

    void advance_to(int32_t timestamp) {
        if (timestamp < current_time_ && current_time_ != BEFORE_START) {
            throw std::logic_error("Clock cannot move backwards: current="
                + std::to_string(current_time_) + " requested=" + std::to_string(timestamp));
        }
        current_time_ = timestamp;
    }

    bool started() const { return current_time_ != BEFORE_START; }

    void reset() { current_time_ = BEFORE_START; }

private:
    int32_t current_time_ = BEFORE_START;
};

}  // namespace qback::engine
