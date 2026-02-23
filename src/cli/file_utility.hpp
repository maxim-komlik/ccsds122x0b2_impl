#pragma once

#include <vector>
#include <filesystem>
#include <cstddef>

bool if_path_is_pattern(const std::filesystem::path& path);
std::vector<std::filesystem::path> expand_pattern(const std::filesystem::path& pattern, size_t max_count = 4096);
