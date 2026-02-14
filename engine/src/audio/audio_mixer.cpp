#include "engine/audio/audio_mixer.h"
#include "engine/audio/audio_engine.h"
#include "engine/core/log.h"

#include <cmath>

namespace Engine {

void AudioMixer::Init() {
    for (u8 i = 0; i < (u8)AudioGroup::Count; i++) {
        s_GroupVolumes[i] = 1.0f;
        s_GroupMuted[i] = false;
    }
    s_Events.clear();
    LOG_INFO("[AudioMixer] 初始化 | %u 个通道组", (u32)AudioGroup::Count);
}

void AudioMixer::Shutdown() {
    LOG_INFO("[AudioMixer] 关闭 | %zu 个音频事件", s_Events.size());
    s_Events.clear();
}

void AudioMixer::SetGroupVolume(AudioGroup group, f32 volume) {
    volume = (volume < 0.0f) ? 0.0f : (volume > 1.0f) ? 1.0f : volume;
    s_GroupVolumes[(u8)group] = volume;
}

f32 AudioMixer::GetGroupVolume(AudioGroup group) {
    return s_GroupVolumes[(u8)group];
}

void AudioMixer::SetGroupMuted(AudioGroup group, bool muted) {
    s_GroupMuted[(u8)group] = muted;
}

bool AudioMixer::IsGroupMuted(AudioGroup group) {
    return s_GroupMuted[(u8)group];
}

f32 AudioMixer::CalculateFinalVolume(AudioGroup group, f32 eventVolume,
                                       f32 distance,
                                       const AttenuationConfig* attenuation) {
    if (s_GroupMuted[(u8)AudioGroup::Master] || s_GroupMuted[(u8)group])
        return 0.0f;

    f32 master = s_GroupVolumes[(u8)AudioGroup::Master];
    f32 groupVol = s_GroupVolumes[(u8)group];
    f32 attGain = 1.0f;

    if (attenuation && distance > 0) {
        attGain = attenuation->Calculate(distance);
    }

    return master * groupVol * eventVolume * attGain;
}

void AudioMixer::RegisterEvent(const std::string& name, const AudioEvent& event) {
    s_Events[name] = event;
    LOG_DEBUG("[AudioMixer] 注册事件: '%s' (组: %u)", name.c_str(), (u32)event.Group);
}

const AudioEvent* AudioMixer::GetEvent(const std::string& name) {
    auto it = s_Events.find(name);
    return (it != s_Events.end()) ? &it->second : nullptr;
}

void AudioMixer::TriggerEvent(const std::string& name, f32 x, f32 y, f32 z) {
    auto* event = GetEvent(name);
    if (!event) {
        LOG_WARN("[AudioMixer] 未知事件: '%s'", name.c_str());
        return;
    }

    // 计算距离
    f32 distance = 0;
    if (event->Spatial) {
        f32 dx = x - s_ListenerPos[0];
        f32 dy = y - s_ListenerPos[1];
        f32 dz = z - s_ListenerPos[2];
        distance = std::sqrt(dx*dx + dy*dy + dz*dz);
    }

    f32 volume = CalculateFinalVolume(event->Group, event->Volume,
                                       distance, event->Spatial ? &event->Attenuation : nullptr);

    if (volume < 0.001f) return;  // 太安静，不播放

    // TODO: 通过 AudioEngine 实际播放，目前记录日志
    LOG_DEBUG("[AudioMixer] 触发: '%s' vol=%.2f dist=%.1f", name.c_str(), volume, distance);
}

void AudioMixer::SetListenerPosition(f32 x, f32 y, f32 z) {
    s_ListenerPos[0] = x;
    s_ListenerPos[1] = y;
    s_ListenerPos[2] = z;
}

u32 AudioMixer::GetEventCount() {
    return (u32)s_Events.size();
}

} // namespace Engine
