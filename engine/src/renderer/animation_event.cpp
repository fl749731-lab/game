#include "engine/renderer/animation_event.h"
#include "engine/core/log.h"

#include <algorithm>

namespace Engine {

void AnimEventTrack::SortEvents() {
    std::sort(Events.begin(), Events.end(),
        [](const AnimEvent& a, const AnimEvent& b) { return a.Time < b.Time; });
}

void AnimEventDispatcher::RegisterHandler(const std::string& eventName,
                                            AnimEventCallback callback) {
    m_Handlers[eventName].push_back(std::move(callback));
}

void AnimEventDispatcher::AddEventTrack(const AnimEventTrack& track) {
    auto copy = track;
    copy.SortEvents();
    m_Tracks[track.ClipName] = std::move(copy);
}

void AnimEventDispatcher::Dispatch(const std::string& clipName,
                                    f32 prevTime, f32 currTime,
                                    u32 entityId) {
    auto trackIt = m_Tracks.find(clipName);
    if (trackIt == m_Tracks.end()) return;

    const auto& events = trackIt->second.Events;

    for (auto& evt : events) {
        bool shouldFire = false;

        if (currTime >= prevTime) {
            // 正常播放
            shouldFire = (evt.Time > prevTime && evt.Time <= currTime);
        } else {
            // 循环回绕 (currTime < prevTime)
            shouldFire = (evt.Time > prevTime || evt.Time <= currTime);
        }

        if (shouldFire) {
            // 调用所有注册该事件名的回调
            auto handlerIt = m_Handlers.find(evt.Name);
            if (handlerIt != m_Handlers.end()) {
                for (auto& callback : handlerIt->second) {
                    callback(evt, entityId);
                }
            }

            LOG_DEBUG("[AnimEvent] 触发: %s @ %.3fs (entity %u)",
                      evt.Name.c_str(), evt.Time, entityId);
        }
    }
}

void AnimEventDispatcher::ClearTracks() {
    m_Tracks.clear();
}

} // namespace Engine
