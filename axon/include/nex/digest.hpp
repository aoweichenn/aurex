#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace nex {

namespace fs = std::filesystem;

// Compute hex-encoded SHA-256 of file contents.
// Returns empty string on failure.
std::string file_digest(const fs::path& path);

// Compute hex-encoded SHA-256 of a string.
std::string hash_string(std::string_view data);

} // namespace nex
