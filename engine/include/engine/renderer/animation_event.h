#pragma once

#include "engine/core/types.h"

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <any>

namespace Engine {

// ── 动画事件 ────────────────────────────────────────────────

struct AnimEvent {
    std::string Name;                    // 事件名 (如 "footstep", "attack_hit", "spawn_particle")
    f32 Time = 0.0f;                     // 触发时间（秒）
    std::unordered_map<std::string, std::string> Params;  // 自定义参数
};

// ── 动画事件轨道 ────────────────────────────────────────────

struct AnimEventTrack {
    std::string ClipName;                // 关联的动画名
    std::vector<AnimEvent> Events;       // 按时间排序的事件

    /// 按时间排序
    void SortEvents();
};

// ── 事件回调 ────────────────────────────────────────────────

using AnimEventCallback = std::function<void(const AnimEvent&, u32 entityId)>;

// ── 动画事件调度器 ──────────────────────────────────────────

class AnimEventDispatcher {
public:
    /// 注册某类事件的全局回调
    void RegisterHandler(const std::string& eventName, AnimEventCallback callback);

    /// 为某个动画剪辑添加事件轨道
    void AddEventTrack(const AnimEventTrack& track);

    /// 检查并触发事件
    /// prevTime/currTime: 上一帧和当前帧的动画时间
    void Dispatch(const std::string& clipName,
                  f32 prevTime, f32 currTime,
                  u32 entityId);

    /// 清除所有事件轨道
    void ClearTracks();

private:
    std::unordered_map<std::string, AnimEventTrack> m_Tracks;   // clipName → track
    std::unordered_map<std::string, std::vector<AnimEventCallback>> m_Handlers;  // eventName → callbacks
};

} // namespace Engine
