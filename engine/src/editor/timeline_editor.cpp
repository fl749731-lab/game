#include "engine/editor/timeline_editor.h"
#include "engine/core/log.h"

#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <cmath>

namespace Engine {

TimelineEditor::TimelineEditor() {}
TimelineEditor::~TimelineEditor() {}

u32 TimelineEditor::AddTrack(const std::string& name, ImU32 color) {
    TimelineTrack t;
    t.Name = name;
    t.Color = color;
    m_Tracks.push_back(t);
    return (u32)m_Tracks.size() - 1;
}

void TimelineEditor::RemoveTrack(u32 index) {
    if (index < m_Tracks.size())
        m_Tracks.erase(m_Tracks.begin() + index);
}

TimelineTrack* TimelineEditor::GetTrack(u32 index) {
    return (index < m_Tracks.size()) ? &m_Tracks[index] : nullptr;
}

void TimelineEditor::Update(f32 dt) {
    if (!m_Playing) return;
    m_CurrentTime += dt * m_PlaybackSpeed;
    if (m_CurrentTime > m_Duration) {
        if (m_Loop) m_CurrentTime = 0;
        else { m_CurrentTime = m_Duration; m_Playing = false; }
    }
}

f32 TimelineEditor::TimeToScreen(f32 time, ImVec2 cp, ImVec2 cs) const {
    return cp.x + HEADER_WIDTH + (time - m_ViewStart) / (m_ViewEnd - m_ViewStart) * (cs.x - HEADER_WIDTH);
}

f32 TimelineEditor::ScreenToTime(f32 sx, ImVec2 cp, ImVec2 cs) const {
    return m_ViewStart + (sx - cp.x - HEADER_WIDTH) / (cs.x - HEADER_WIDTH) * (m_ViewEnd - m_ViewStart);
}

void TimelineEditor::Render(const char* title) {
    if (!ImGui::Begin(title)) { ImGui::End(); return; }

    RenderControls();
    ImGui::Separator();

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    if (canvasSize.x < 100) canvasSize.x = 100;
    if (canvasSize.y < 50) canvasSize.y = 50;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 背景
    dl->AddRectFilled(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(25, 25, 30, 255));

    RenderTimeline(dl, canvasPos, canvasSize);
    RenderTracks(dl, canvasPos, canvasSize);

    // Invisible button
    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("##TimelineCanvas", canvasSize);
    HandleInput(canvasPos, canvasSize);

    ImGui::End();
}

void TimelineEditor::RenderControls() {
    if (ImGui::Button(m_Playing ? "⏸" : "▶")) {
        m_Playing ? Pause() : Play();
    }
    ImGui::SameLine();
    if (ImGui::Button("⏹")) Stop();
    ImGui::SameLine();
    ImGui::Checkbox("循环", &m_Loop);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::DragFloat("速度", &m_PlaybackSpeed, 0.05f, 0.1f, 5.0f);
    ImGui::SameLine();
    ImGui::Text("时间: %.2f / %.2f", m_CurrentTime, m_Duration);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::DragFloat("时长", &m_Duration, 0.5f, 1.0f, 300.0f);
}

void TimelineEditor::RenderTimeline(ImDrawList* dl, ImVec2 cp, ImVec2 cs) {
    // 时间刻度
    f32 timeRange = m_ViewEnd - m_ViewStart;
    f32 step = 1.0f;
    if (timeRange > 20) step = 5.0f;
    if (timeRange > 100) step = 10.0f;

    for (f32 t = std::ceil(m_ViewStart / step) * step; t <= m_ViewEnd; t += step) {
        f32 sx = TimeToScreen(t, cp, cs);
        dl->AddLine(ImVec2(sx, cp.y), ImVec2(sx, cp.y + cs.y),
                    IM_COL32(50, 50, 60, 150));
        char label[16];
        snprintf(label, sizeof(label), "%.1f", t);
        dl->AddText(ImVec2(sx + 2, cp.y + 2), IM_COL32(140, 140, 160, 200), label);
    }

    // 播放头
    f32 phX = TimeToScreen(m_CurrentTime, cp, cs);
    if (phX >= cp.x + HEADER_WIDTH && phX <= cp.x + cs.x) {
        dl->AddLine(ImVec2(phX, cp.y), ImVec2(phX, cp.y + cs.y),
                    IM_COL32(255, 80, 80, 220), 2.0f);
        ImVec2 tri[3] = {
            {phX - 6, cp.y}, {phX + 6, cp.y}, {phX, cp.y + 8}
        };
        dl->AddTriangleFilled(tri[0], tri[1], tri[2], IM_COL32(255, 80, 80, 240));
    }
}

void TimelineEditor::RenderTracks(ImDrawList* dl, ImVec2 cp, ImVec2 cs) {
    f32 headerEnd = cp.x + HEADER_WIDTH;
    f32 startY = cp.y + 20;  // 刻度行之后

    for (size_t i = 0; i < m_Tracks.size(); i++) {
        auto& track = m_Tracks[i];
        f32 ty = startY + i * TRACK_HEIGHT;

        // 轨道背景
        ImU32 bgColor = ((i32)i == m_SelectedTrack) ?
            IM_COL32(50, 50, 70, 200) : IM_COL32(35, 35, 40, 200);
        dl->AddRectFilled(ImVec2(cp.x, ty), ImVec2(cp.x + cs.x, ty + TRACK_HEIGHT), bgColor);

        // 分割线
        dl->AddLine(ImVec2(cp.x, ty + TRACK_HEIGHT),
                    ImVec2(cp.x + cs.x, ty + TRACK_HEIGHT),
                    IM_COL32(60, 60, 70, 200));

        // 轨道名称
        ImVec4 nameColor = ImGui::ColorConvertU32ToFloat4(track.Color);
        if (!track.Visible || track.Muted) nameColor.w = 0.4f;
        dl->AddText(ImVec2(cp.x + 5, ty + 6),
                    ImGui::ColorConvertFloat4ToU32(nameColor),
                    track.Name.c_str());

        // 关键帧标记
        if (track.Visible) {
            for (auto& key : track.Curve.Keys) {
                f32 kx = TimeToScreen(key.Time, cp, cs);
                if (kx < headerEnd || kx > cp.x + cs.x) continue;

                f32 ky = ty + TRACK_HEIGHT * 0.5f;
                ImVec2 diamond[4] = {
                    {kx, ky - 4}, {kx + 4, ky}, {kx, ky + 4}, {kx - 4, ky}
                };
                dl->AddConvexPolyFilled(diamond, 4, track.Color);
            }
        }
    }

    // 轨道头分割线
    dl->AddLine(ImVec2(headerEnd, cp.y), ImVec2(headerEnd, cp.y + cs.y),
                IM_COL32(70, 70, 80, 255));
}

void TimelineEditor::HandleInput(ImVec2 cp, ImVec2 cs) {
    if (!ImGui::IsItemHovered()) return;
    ImGuiIO& io = ImGui::GetIO();

    // 点击设置播放头
    if (ImGui::IsMouseClicked(0)) {
        f32 mouseTime = ScreenToTime(io.MousePos.x, cp, cs);
        if (mouseTime >= 0 && mouseTime <= m_Duration) {
            m_CurrentTime = mouseTime;
        }

        // 选中轨道
        f32 startY = cp.y + 20;
        i32 trackIdx = (i32)((io.MousePos.y - startY) / TRACK_HEIGHT);
        if (trackIdx >= 0 && trackIdx < (i32)m_Tracks.size())
            m_SelectedTrack = trackIdx;
    }

    // 拖拽播放头
    if (ImGui::IsMouseDragging(0)) {
        f32 mouseTime = ScreenToTime(io.MousePos.x, cp, cs);
        m_CurrentTime = std::clamp(mouseTime, 0.0f, m_Duration);
    }

    // 缩放
    if (io.MouseWheel != 0) {
        f32 center = ScreenToTime(io.MousePos.x, cp, cs);
        f32 factor = 1.0f - io.MouseWheel * 0.1f;
        m_ViewStart = center + (m_ViewStart - center) * factor;
        m_ViewEnd = center + (m_ViewEnd - center) * factor;
    }

    // 中键平移
    if (ImGui::IsMouseDragging(2)) {
        f32 dt = -io.MouseDelta.x / (cs.x - HEADER_WIDTH) * (m_ViewEnd - m_ViewStart);
        m_ViewStart += dt;
        m_ViewEnd += dt;
    }
}

} // namespace Engine
