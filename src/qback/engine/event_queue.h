#pragma once

#include <cstdint>
#include <queue>
#include <vector>

#include "qback/domain/event.h"

namespace qback::engine {

class EventQueue {
public:
    void push(domain::Event event) {
        heap_.push(std::move(event));
    }

    domain::Event pop() {
        auto event = std::move(const_cast<domain::Event&>(heap_.top()));
        heap_.pop();
        return event;
    }

    const domain::Event& peek() const {
        return heap_.top();
    }

    bool empty() const { return heap_.empty(); }
    std::size_t size() const { return heap_.size(); }

    uint64_t next_sequence() { return seq_counter_++; }

private:
    // std::priority_queue is a max-heap; Event::operator> gives us min-heap behavior
    std::priority_queue<domain::Event, std::vector<domain::Event>,
                        std::greater<domain::Event>> heap_;
    uint64_t seq_counter_ = 0;
};

}  // namespace qback::engine
