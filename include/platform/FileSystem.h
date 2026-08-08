#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace platform
{
    std::filesystem::path executable_directory();

    // Both report the reason to stderr and return false on failure
    bool write_text_file(const std::filesystem::path& path, std::string_view contents);
    bool read_text_file(const std::filesystem::path& path, std::string& contents);
}
