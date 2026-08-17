#pragma once

#include <vector>

#include "qback/domain/bar.h"
#include "qback/domain/event.h"
#include "qback/engine/event_queue.h"

namespace qback::engine {

// Converts loaded instruments into chronological MarketData events and
// enqueues them. Each bar for each symbol becomes one event, timestamped
// at the bar's date. Within the same date, symbols are ordered alphabetically
// via the sequence counter for determinism.
inline void load_bars_into_queue(const std::vector<domain::Instrument>& instruments,
                                  EventQueue& queue) {
    struct BarEntry {
        int32_t date;
        const std::string* symbol;
        const domain::Bar* bar;
    };

    std::vector<BarEntry> all_bars;
    for (const auto& inst : instruments) {
        for (const auto& bar : inst.bars) {
            all_bars.push_back({bar.date.days_since_epoch, &inst.symbol, &bar});
        }
    }

    // Sort by (date, symbol) for deterministic sequence assignment
    std::sort(all_bars.begin(), all_bars.end(), [](const auto& a, const auto& b) {
        if (a.date != b.date) return a.date < b.date;
        return *a.symbol < *b.symbol;
    });

    for (const auto& entry : all_bars) {
        auto seq = queue.next_sequence();
        queue.push(domain::make_market_data_event(entry.date, seq, *entry.symbol, *entry.bar));
    }
}

}  // namespace qback::engine
