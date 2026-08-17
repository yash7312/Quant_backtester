#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "qback/domain/bar.h"

namespace qback::data {

domain::Instrument read_parquet(const std::filesystem::path& path);

std::vector<domain::Instrument> read_all_parquet(const std::filesystem::path& directory);

}  // namespace qback::data
