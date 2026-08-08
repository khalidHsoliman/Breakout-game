#pragma once

#include <filesystem>

namespace platform
{
    struct AudioDevice;

    enum class Sound
    {
        BrickHit,
        PaddleBounce
    };

    // Loads three files from a directory and plays them.
    class Audio
    {
    public:
        Audio() = default;
        ~Audio();

        Audio(const Audio&) = delete;
        Audio& operator=(const Audio&) = delete;

        // Reports the reason to stderr and returns false.
        bool init(const std::filesystem::path& asset_directory);

        // Both are no-ops if init failed, so callers need no guard.
        void play(Sound sound);
        void start_music();

    private:
        AudioDevice* m_device = nullptr;
    };
}
