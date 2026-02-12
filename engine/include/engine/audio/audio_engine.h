#pragma once

#include "engine/core/types.h"
#include <string>

namespace Engine {

// ── 音频引擎 ────────────────────────────────────────────────
// 基于 miniaudio 的简易音频系统
// 支持音乐播放（带循环）和一次性音效

class AudioEngine {
public:
    /// 初始化音频设备
    static bool Init();

    /// 清理
    static void Shutdown();

    /// 播放背景音乐 (自动循环)
    static void PlayMusic(const std::string& filepath, f32 volume = 1.0f);

    /// 停止背景音乐
    static void StopMusic();

    /// 暂停/恢复音乐
    static void PauseMusic();
    static void ResumeMusic();
    static bool IsMusicPlaying();

    /// 设置音乐音量 [0.0 ~ 1.0]
    static void SetMusicVolume(f32 volume);

    /// 播放一次性音效 (fire-and-forget)
    static void PlaySFX(const std::string& filepath, f32 volume = 1.0f);

    /// 设置全局主音量 [0.0 ~ 1.0]
    static void SetMasterVolume(f32 volume);

    /// 是否已初始化
    static bool IsInitialized();
};

} // namespace Engine
