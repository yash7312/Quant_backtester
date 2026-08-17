#include "parquet_reader.h"

#include <arrow/api.h>
#include <arrow/io/file.h>
#include <parquet/arrow/reader.h>

#include <iostream>
#include <stdexcept>

namespace qback::data {

namespace {

template <typename ArrayType>
const ArrayType& checked_column(const arrow::Table& table, const std::string& name) {
    auto col = table.GetColumnByName(name);
    if (!col) {
        throw std::runtime_error("Missing column: " + name);
    }
    if (col->num_chunks() != 1) {
        throw std::runtime_error("Expected single chunk for column: " + name);
    }
    auto result = std::dynamic_pointer_cast<ArrayType>(col->chunk(0));
    if (!result) {
        throw std::runtime_error("Type mismatch for column: " + name);
    }
    return *result;
}

std::string extract_symbol(const arrow::Table& table, const std::filesystem::path& path) {
    auto metadata = table.schema()->metadata();
    if (metadata) {
        auto idx = metadata->FindKey("symbol");
        if (idx >= 0) {
            return metadata->value(idx);
        }
    }
    return path.stem().string();
}

}  // namespace

domain::Instrument read_parquet(const std::filesystem::path& path) {
    auto infile_result = arrow::io::ReadableFile::Open(path.string());
    if (!infile_result.ok()) {
        throw std::runtime_error("Cannot open " + path.string() + ": " + infile_result.status().ToString());
    }

    auto reader_result = parquet::arrow::OpenFile(*infile_result, arrow::default_memory_pool());
    if (!reader_result.ok()) {
        throw std::runtime_error("Cannot read parquet " + path.string() + ": " + reader_result.status().ToString());
    }
    auto reader = std::move(*reader_result);

    auto table_result = reader->ReadTable();
    if (!table_result.ok()) {
        throw std::runtime_error("Cannot read table from " + path.string() + ": " + table_result.status().ToString());
    }
    auto table = *table_result;

    auto combined_result = table->CombineChunks();
    if (!combined_result.ok()) {
        throw std::runtime_error("Cannot combine chunks: " + combined_result.status().ToString());
    }
    table = *combined_result;

    const auto& dates = checked_column<arrow::Date32Array>(*table, "date");
    const auto& opens = checked_column<arrow::DoubleArray>(*table, "open");
    const auto& highs = checked_column<arrow::DoubleArray>(*table, "high");
    const auto& lows = checked_column<arrow::DoubleArray>(*table, "low");
    const auto& closes = checked_column<arrow::DoubleArray>(*table, "close");
    const auto& volumes = checked_column<arrow::Int64Array>(*table, "volume");
    const auto& adj_closes = checked_column<arrow::DoubleArray>(*table, "adjusted_close");

    domain::Instrument instrument;
    instrument.symbol = extract_symbol(*table, path);
    instrument.bars.reserve(static_cast<size_t>(table->num_rows()));

    for (int64_t i = 0; i < table->num_rows(); ++i) {
        instrument.bars.push_back({
            .date = {dates.Value(i)},
            .open = opens.Value(i),
            .high = highs.Value(i),
            .low = lows.Value(i),
            .close = closes.Value(i),
            .volume = volumes.Value(i),
            .adjusted_close = adj_closes.Value(i),
        });
    }

    return instrument;
}

std::vector<domain::Instrument> read_all_parquet(const std::filesystem::path& directory) {
    std::vector<domain::Instrument> instruments;

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.path().extension() == ".parquet") {
            instruments.push_back(read_parquet(entry.path()));
        }
    }

    std::sort(instruments.begin(), instruments.end(),
              [](const auto& a, const auto& b) { return a.symbol < b.symbol; });

    return instruments;
}

}  // namespace qback::data
