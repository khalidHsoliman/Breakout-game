#include <miniaudio.h>

#include <cstdio>

#include "platform/Audio.h"

// The engine and sound API used here, and the difference between decoding a
// short effect and streaming a long track, are documented at:
// https://miniaud.io/docs/manual
//
// Which sounds a Breakout clone wants, and when to play them, follows:
// https://learnopengl.com/In-Practice/2D-Game/Audio

namespace platform
{
    namespace
    {
        constexpr const char* MUSIC_FILE = "music.mp3";
        constexpr const char* BRICK_FILE = "brick.wav";
        constexpr const char* PADDLE_FILE = "paddle.wav";

        constexpr float MUSIC_VOLUME = 0.35f;
        constexpr float EFFECT_VOLUME = 0.70f;
    }

    struct AudioDevice
    {
        ma_engine engine{};
        ma_sound music{};
        ma_sound brick{};
        ma_sound paddle{};

        bool music_loaded = false;
    };

    namespace
    {

        bool load_effect(ma_engine& engine, const std::filesystem::path& path, ma_sound& sound)
        {
            const ma_uint32 flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION;

            const ma_result result =
                ma_sound_init_from_file(&engine, path.string().c_str(), flags, nullptr, nullptr, &sound);

            if (result != MA_SUCCESS)
            {
                std::fprintf(stderr, "Failed to load %s.\n", path.string().c_str());
                return false;
            }

            ma_sound_set_volume(&sound, EFFECT_VOLUME);
            return true;
        }
    }

    Audio::~Audio()
    {
        if (m_device == nullptr)
        {
            return;
        }

        // Sounds hold a reference to the engine, so they go first.
        ma_sound_uninit(&m_device->paddle);
        ma_sound_uninit(&m_device->brick);

        if (m_device->music_loaded)
        {
            ma_sound_uninit(&m_device->music);
        }

        ma_engine_uninit(&m_device->engine);

        delete m_device;
    }

    bool Audio::init(const std::filesystem::path& asset_directory)
    {
        AudioDevice* device = new AudioDevice{};

        if (ma_engine_init(nullptr, &device->engine) != MA_SUCCESS)
        {
            std::fprintf(stderr, "Failed to open an audio device.\n");
            delete device;
            return false;
        }

        if (!load_effect(device->engine, asset_directory / BRICK_FILE, device->brick) ||
            !load_effect(device->engine, asset_directory / PADDLE_FILE, device->paddle))
        {
            ma_engine_uninit(&device->engine);
            delete device;
            return false;
        }

        const std::filesystem::path music_path = asset_directory / MUSIC_FILE;
        const ma_uint32 music_flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION;

        if (ma_sound_init_from_file(&device->engine, music_path.string().c_str(),
                                    music_flags, nullptr, nullptr, &device->music) == MA_SUCCESS)
        {
            ma_sound_set_looping(&device->music, MA_TRUE);
            ma_sound_set_volume(&device->music, MUSIC_VOLUME);
            device->music_loaded = true;
        }
        else
        {
            std::fprintf(stderr, "Failed to load %s. Continuing without music.\n",
                         music_path.string().c_str());
        }

        m_device = device;
        return true;
    }

    void Audio::play(Sound sound)
    {
        if (m_device == nullptr)
        {
            return;
        }

        ma_sound& target = (sound == Sound::BrickHit) ? m_device->brick : m_device->paddle;

        // Rewound
        ma_sound_seek_to_pcm_frame(&target, 0);
        ma_sound_start(&target);
    }

    void Audio::start_music()
    {
        if (m_device == nullptr || !m_device->music_loaded)
        {
            return;
        }

        ma_sound_start(&m_device->music);
    }
}
