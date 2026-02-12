#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "engine/audio/audio_engine.h"
#include "engine/core/log.h"

#include <mutex>
#include <vector>
#include <algorithm>

namespace Engine {

// ── 内部状态 ────────────────────────────────────────────────

static ma_engine* s_Engine = nullptr;
static ma_sound*  s_MusicSound = nullptr;
static bool       s_Initialized = false;
static f32        s_MasterVolume = 1.0f;
static f32        s_MusicVolume  = 1.0f;
static std::string s_CurrentMusic;

// SFX 管理 — fire-and-forget，完成后自动清理
struct SFXEntry {
    ma_sound* Sound = nullptr;
};
static std::vector<SFXEntry> s_ActiveSFX;
static std::mutex s_SFXMutex;
static u32 s_SFXPlayCount = 0;  // 用于控制清理频率

// ── 初始化 ──────────────────────────────────────────────────

bool AudioEngine::Init() {
    if (s_Initialized) return true;

    s_Engine = new ma_engine();
    ma_engine_config config = ma_engine_config_init();

    if (ma_engine_init(&config, s_Engine) != MA_SUCCESS) {
        LOG_ERROR("[AudioEngine] 初始化失败！");
        delete s_Engine;
        s_Engine = nullptr;
        return false;
    }

    s_Initialized = true;
    LOG_INFO("[AudioEngine] 初始化完成");
    return true;
}

void AudioEngine::Shutdown() {
    if (!s_Initialized) return;

    StopMusic();

    // 清理 SFX
    {
        std::lock_guard<std::mutex> lock(s_SFXMutex);
        for (auto& sfx : s_ActiveSFX) {
            if (sfx.Sound) {
                ma_sound_uninit(sfx.Sound);
                delete sfx.Sound;
            }
        }
        s_ActiveSFX.clear();
    }

    if (s_Engine) {
        ma_engine_uninit(s_Engine);
        delete s_Engine;
        s_Engine = nullptr;
    }

    s_Initialized = false;
    LOG_INFO("[AudioEngine] 已关闭");
}

// ── 音乐 ────────────────────────────────────────────────────

void AudioEngine::PlayMusic(const std::string& filepath, f32 volume) {
    if (!s_Initialized) return;

    // 先停止当前音乐
    StopMusic();

    s_MusicSound = new ma_sound();
    if (ma_sound_init_from_file(s_Engine, filepath.c_str(),
                                MA_SOUND_FLAG_STREAM, nullptr, nullptr,
                                s_MusicSound) != MA_SUCCESS) {
        LOG_ERROR("[AudioEngine] 无法加载音乐: %s", filepath.c_str());
        delete s_MusicSound;
        s_MusicSound = nullptr;
        return;
    }

    ma_sound_set_looping(s_MusicSound, MA_TRUE);
    ma_sound_set_volume(s_MusicSound, volume * s_MasterVolume);
    ma_sound_start(s_MusicSound);
    s_MusicVolume = volume;
    s_CurrentMusic = filepath;
    LOG_INFO("[AudioEngine] 播放音乐: %s", filepath.c_str());
}

void AudioEngine::StopMusic() {
    if (s_MusicSound) {
        ma_sound_stop(s_MusicSound);
        ma_sound_uninit(s_MusicSound);
        delete s_MusicSound;
        s_MusicSound = nullptr;
        s_CurrentMusic.clear();
    }
}

void AudioEngine::PauseMusic() {
    if (s_MusicSound) ma_sound_stop(s_MusicSound);
}

void AudioEngine::ResumeMusic() {
    if (s_MusicSound) ma_sound_start(s_MusicSound);
}

bool AudioEngine::IsMusicPlaying() {
    return s_MusicSound && ma_sound_is_playing(s_MusicSound);
}

void AudioEngine::SetMusicVolume(f32 volume) {
    s_MusicVolume = volume;
    if (s_MusicSound) {
        ma_sound_set_volume(s_MusicSound, volume * s_MasterVolume);
    }
}

// ── 音效 ────────────────────────────────────────────────────

void AudioEngine::PlaySFX(const std::string& filepath, f32 volume) {
    if (!s_Initialized) return;

    // 每16次播放清理一次已完成音效（避免每次都遍历）
    if ((++s_SFXPlayCount & 0xF) == 0) {
        std::lock_guard<std::mutex> lock(s_SFXMutex);
        s_ActiveSFX.erase(
            std::remove_if(s_ActiveSFX.begin(), s_ActiveSFX.end(),
                [](SFXEntry& e) {
                    if (e.Sound && ma_sound_at_end(e.Sound)) {
                        ma_sound_uninit(e.Sound);
                        delete e.Sound;
                        return true;
                    }
                    return false;
                }),
            s_ActiveSFX.end());
    }

    // 使用 ma_sound 创建并设置音量
    auto* sound = new ma_sound();
    if (ma_sound_init_from_file(s_Engine, filepath.c_str(),
                                MA_SOUND_FLAG_DECODE, nullptr, nullptr,
                                sound) != MA_SUCCESS) {
        LOG_WARN("[AudioEngine] 音效加载失败: %s", filepath.c_str());
        delete sound;
        return;
    }
    ma_sound_set_volume(sound, volume * s_MasterVolume);
    ma_sound_start(sound);

    // 注册到活跃列表以便后续清理
    {
        std::lock_guard<std::mutex> lock(s_SFXMutex);
        s_ActiveSFX.push_back({sound});
    }
}

// ── 全局音量 ────────────────────────────────────────────────

void AudioEngine::SetMasterVolume(f32 volume) {
    s_MasterVolume = volume;
    if (s_Engine) {
        ma_engine_set_volume(s_Engine, volume);
    }
    // 更新音乐音量
    if (s_MusicSound) {
        ma_sound_set_volume(s_MusicSound, s_MusicVolume * s_MasterVolume);
    }
}

bool AudioEngine::IsInitialized() {
    return s_Initialized;
}

} // namespace Engine
