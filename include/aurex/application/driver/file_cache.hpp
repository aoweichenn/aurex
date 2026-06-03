#pragma once

#include <aurex/infrastructure/base/result.hpp>

#include <filesystem>
#include <string>

namespace aurex::driver {

[[nodiscard]] base::Result<std::string> read_text_file(const std::filesystem::path& path);
void clear_file_cache();

} // namespace aurex::driver
