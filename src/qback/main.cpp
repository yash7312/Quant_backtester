#include <filesystem>
#include <iomanip>
#include <iostream>

#include "qback/data/parquet_reader.h"

int main(int argc, char* argv[]) {
    std::filesystem::path data_dir = "data/raw";
    if (argc > 1) {
        data_dir = argv[1];
    }

    if (!std::filesystem::exists(data_dir)) {
        std::cerr << "Data directory not found: " << data_dir << "\n"
                  << "Run 'python scripts/fetch_data.py' first.\n";
        return 1;
    }

    std::cout << "Reading parquet files from: " << std::filesystem::absolute(data_dir) << "\n\n";

    auto instruments = qback::data::read_all_parquet(data_dir);

    if (instruments.empty()) {
        std::cerr << "No .parquet files found in " << data_dir << "\n";
        return 1;
    }

    std::cout << std::left << std::setw(8) << "Symbol"
              << std::right << std::setw(7) << "Rows"
              << "  " << std::setw(12) << "First date"
              << "  " << std::setw(12) << "Last date"
              << "  " << std::setw(10) << "Low"
              << "  " << std::setw(10) << "High"
              << "\n";
    std::cout << std::string(65, '-') << "\n";

    for (const auto& inst : instruments) {
        double min_low = inst.bars.front().low;
        double max_high = inst.bars.front().high;
        for (const auto& bar : inst.bars) {
            min_low = std::min(min_low, bar.low);
            max_high = std::max(max_high, bar.high);
        }

        std::cout << std::left << std::setw(8) << inst.symbol
                  << std::right << std::setw(7) << inst.size()
                  << "  " << std::setw(12) << inst.front().date.to_string()
                  << "  " << std::setw(12) << inst.back().date.to_string()
                  << "  " << std::fixed << std::setprecision(2)
                  << std::setw(10) << min_low
                  << "  " << std::setw(10) << max_high
                  << "\n";
    }

    std::cout << "\nData access verified: " << instruments.size() << " instruments loaded.\n";
    return 0;
}
