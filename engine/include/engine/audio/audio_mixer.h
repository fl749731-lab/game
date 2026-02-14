#pragma once

#include "engine/core/types.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <cmath>

namespace Engine {

// ── 音频通道组 ──────────────────────────────────────────────

enum class AudioGroup : u8 {
    Master = 0,
    BGM    = 1,
    SFX    = 2,
    UI     = 3,
    Voice  = 4,
    Ambient = 5,
    Count  = 6
};

// ── 距离衰减模型 ────────────────────────────────────────────

enum class AttenuationModel : u8 {
    None,           // 无衰减
    Linear,         // 线性衰减
    InverseDistance, // 反距离衰减 (1/d)
    ExponentialDistance // 指数衰减
};

struct AttenuationConfig {
    AttenuationModel Model = AttenuationModel::InverseDistance;
    f32 RefDistance = 1.0f;    // 不衰减的参考距离
    f32 MaxDistance = 50.0f;   // 最大听觉距离
    f32 Rolloff = 1.0f;       // 衰减速率

    /// 根据距离计算增益倍数 [0,1]
    f32 Calculate(f32 distance) const {
        if (distance <= RefDistance) return 1.0f;
        if (distance >= MaxDistance) return 0.0f;

        switch (Model) {
            case AttenuationModel::None:
                return 1.0f;
            case AttenuationModel::Linear: {
                f32 t = (distance - RefDistance) / (MaxDistance - RefDistance);
                return 1.0f - t;
            }
            case AttenuationModel::InverseDistance:
                return RefDistance / (RefDistance + Rolloff * (distance - RefDistance));
            case AttenuationModel::ExponentialDistance:
                return std::pow(distance / RefDistance, -Rolloff);
        }
        return 1.0f;
    }
};

// ── 音频事件 ────────────────────────────────────────────────

struct AudioEvent {
    std::string Name;
    std::string SoundFile;     // 音频文件路径
    AudioGroup Group = AudioGroup::SFX;
    f32 Volume = 1.0f;
    f32 Pitch = 1.0f;
    bool Loop = false;
    bool Spatial = false;      // 3D 空间音效
    AttenuationConfig Attenuation;
};

// ── AudioMixer ──────────────────────────────────────────────
// 通道组混合器 — 独立控制各组音量
// 支持: 音量层级 (Master × Group × Event)
//       距离衰减 (3D 空间音效)
//       音频事件 (触发式管理)

class AudioMixer {
public:
    static void Init();
    static void Shutdown();

    /// 通道组音量控制 [0, 1]
    static void SetGroupVolume(AudioGroup group, f32 volume);
    static f32 GetGroupVolume(AudioGroup group);

    /// 静音控制
    static void SetGroupMuted(AudioGroup group, bool muted);
    static bool IsGroupMuted(AudioGroup group);

    /// 计算最终音量: Master × Group × Event × Attenuation
    static f32 CalculateFinalVolume(AudioGroup group, f32 eventVolume,
                                     f32 distance = 0.0f,
                                     const AttenuationConfig* attenuation = nullptr);

    /// 音频事件管理
    static void RegisterEvent(const std::string& name, const AudioEvent& event);
    static const AudioEvent* GetEvent(const std::string& name);

    /// 触发音频事件 (位置用于 3D 自动衰减)
    static void TriggerEvent(const std::string& name, f32 x = 0, f32 y = 0, f32 z = 0);

    /// 设置听者位置 (用于 3D 衰减计算)
    static void SetListenerPosition(f32 x, f32 y, f32 z);

    /// 统计
    static u32 GetEventCount();

private:
    inline static f32 s_GroupVolumes[(u8)AudioGroup::Count] = {1,1,1,1,1,1};
    inline static bool s_GroupMuted[(u8)AudioGroup::Count] = {};
    inline static f32 s_ListenerPos[3] = {};
    inline static std::unordered_map<std::string, AudioEvent> s_Events;
};

} // namespace Engine
