#pragma once

#include "engine/core/types.h"
#include "engine/editor/curve_editor.h"

#include <imgui.h>
#include <string>
#include <vector>

namespace Engine {

// ── 时间轴编辑器 ────────────────────────────────────────────
// 功能:
//   1. 多轨道时间轴 (每轨道关联一个 AnimCurve)
//   2. 关键帧条状显示
//   3. 播放控制 (播放/暂停/停止/跳转)
//   4. 缩放拖拽
//   5. 与 CurveEditor 集成

struct TimelineTrack {
    std::string Name;
    ImU32 Color = IM_COL32(120, 180, 255, 255);
    AnimCurve Curve;
    bool Visible = true;
    bool Locked = false;
    bool Solo = false;
    bool Muted = false;
};

class TimelineEditor {
public:
    TimelineEditor();
    ~TimelineEditor();

    void Render(const char* title = "时间轴");

    // 轨道管理
    u32  AddTrack(const std::string& name, ImU32 color = IM_COL32(120, 180, 255, 255));
    void RemoveTrack(u32 index);
    TimelineTrack* GetTrack(u32 index);

    // 播放控制
    void Play()   { m_Playing = true; }
    void Pause()  { m_Playing = false; }
    void Stop()   { m_Playing = false; m_CurrentTime = 0; }
    void SetTime(f32 time) { m_CurrentTime = time; }
    f32  GetTime() const { return m_CurrentTime; }
    bool IsPlaying() const { return m_Playing; }

    // 范围
    void SetDuration(f32 duration) { m_Duration = duration; }
    f32  GetDuration() const { return m_Duration; }

    // 每帧更新
    void Update(f32 dt);

private:
    void RenderControls();
    void RenderTracks(ImDrawList* dl, ImVec2 canvasPos, ImVec2 canvasSize);
    void RenderTimeline(ImDrawList* dl, ImVec2 canvasPos, ImVec2 canvasSize);
    void HandleInput(ImVec2 canvasPos, ImVec2 canvasSize);

    f32 TimeToScreen(f32 time, ImVec2 canvasPos, ImVec2 canvasSize) const;
    f32 ScreenToTime(f32 screenX, ImVec2 canvasPos, ImVec2 canvasSize) const;

    std::vector<TimelineTrack> m_Tracks;
    f32 m_CurrentTime = 0;
    f32 m_Duration = 10.0f;
    f32 m_ViewStart = 0, m_ViewEnd = 10.0f;
    bool m_Playing = false;
    bool m_Loop = false;
    f32 m_PlaybackSpeed = 1.0f;
    i32 m_SelectedTrack = -1;

    static constexpr f32 TRACK_HEIGHT = 30.0f;
    static constexpr f32 HEADER_WIDTH = 150.0f;
};

} // namespace Engine
