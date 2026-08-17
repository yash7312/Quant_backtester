#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "qback/domain/event.h"
#include "qback/engine/clock.h"
#include "qback/engine/event_queue.h"

namespace qback::engine {

using EventHandler = std::function<void(const domain::Event&)>;

class Engine {
public:
    void set_handler(domain::EventType type, EventHandler handler) {
        handlers_[static_cast<uint8_t>(type)] = std::move(handler);
    }

    EventQueue& queue() { return queue_; }
    const Clock& clock() const { return clock_; }

    uint64_t next_sequence() { return queue_.next_sequence(); }

    void push_event(domain::Event event) {
        queue_.push(std::move(event));
    }

    struct RunStats {
        uint64_t events_processed = 0;
        int32_t first_timestamp = 0;
        int32_t last_timestamp = 0;
    };

    RunStats run() {
        RunStats stats{};
        bool first = true;

        while (!queue_.empty()) {
            auto event = queue_.pop();
            clock_.advance_to(event.key.timestamp);

            if (first) {
                stats.first_timestamp = event.key.timestamp;
                first = false;
            }
            stats.last_timestamp = event.key.timestamp;

            auto it = handlers_.find(static_cast<uint8_t>(event.type));
            if (it != handlers_.end()) {
                it->second(event);
            }

            stats.events_processed++;
        }

        return stats;
    }

private:
    EventQueue queue_;
    Clock clock_;
    std::unordered_map<uint8_t, EventHandler> handlers_;
};

}  // namespace qback::engine
