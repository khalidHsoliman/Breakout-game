#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

#include "platform/FileSystem.h"

// The return value of GetModuleFileNameW, and its behaviour when the buffer is
// too small, are documented here. The code below follows from it:
// https://learn.microsoft.com/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulefilenamew

namespace platform
{
    std::filesystem::path executable_directory()
    {
        std::wstring buffer(MAX_PATH, L'\0');

        while (true)
        {
            // GetModuleFileNameW returns the number of characters written, not
            // including the null terminator. nullptr is passed to get the full
            // path of the current executable
            const DWORD written =
                GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

            if (written == 0)
            {
                std::fprintf(stderr, "Failed to locate the executable.\n");
                return {};
            }

            // Filling the buffer exactly means the path was truncated: the
            // limit is a convention, not a maximum.
            if (written < buffer.size())
            {
                buffer.resize(written);
                break;
            }

            buffer.resize(buffer.size() * 2);
        }

        return std::filesystem::path(buffer).parent_path();
    }

    bool write_text_file(const std::filesystem::path& path, std::string_view contents)
    {
        // Binary, so the bytes on disk are exactly the bytes given.
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            std::fprintf(stderr, "Failed to open %s for writing.\n", path.string().c_str());
            return false;
        }

        file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        file.close();
        if (!file)
        {
            std::fprintf(stderr, "Failed to write %s.\n", path.string().c_str());
            return false;
        }

        return true;
    }

    bool read_text_file(const std::filesystem::path& path, std::string& contents)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            std::fprintf(stderr, "Failed to open %s for reading.\n", path.string().c_str());
            return false;
        }

        contents.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

        if (file.bad())
        {
            std::fprintf(stderr, "Failed to read %s.\n", path.string().c_str());
            return false;
        }

        return true;
    }
}
