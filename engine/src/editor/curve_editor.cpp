#include "engine/editor/curve_editor.h"
#include "engine/core/log.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Engine {

// ── Hermite 插值 ────────────────────────────────────────────

f32 AnimCurve::Evaluate(f32 time) const {
    if (Keys.empty()) return 0;
    if (Keys.size() == 1) return Keys[0].Value;
    if (time <= Keys.front().Time) return Keys.front().Value;
    if (time >= Keys.back().Time) return Keys.back().Value;

    // 查找区间
    for (size_t i = 0; i < Keys.size() - 1; i++) {
        const auto& k0 = Keys[i];
        const auto& k1 = Keys[i + 1];
        if (time >= k0.Time && time <= k1.Time) {
            // Constant 模式
            if (k0.Mode == CurveKeyframe::Constant) return k0.Value;
            // Linear 模式
            if (k0.Mode == CurveKeyframe::Linear) {
                f32 t = (time - k0.Time) / (k1.Time - k0.Time);
                return k0.Value + t * (k1.Value - k0.Value);
            }
            // Hermite 样条
            f32 dt = k1.Time - k0.Time;
            f32 t = (time - k0.Time) / dt;
            f32 t2 = t * t, t3 = t2 * t;

            f32 h00 =  2*t3 - 3*t2 + 1;
            f32 h10 =    t3 - 2*t2 + t;
            f32 h01 = -2*t3 + 3*t2;
            f32 h11 =    t3 -   t2;

            return h00 * k0.Value + h10 * dt * k0.OutTangent
                 + h01 * k1.Value + h11 * dt * k1.InTangent;
        }
    }
    return Keys.back().Value;
}

// ── Editor ──────────────────────────────────────────────────

CurveEditor::CurveEditor() {}
CurveEditor::~CurveEditor() {}

u32 CurveEditor::AddCurve(const std::string& name, ImU32 color) {
    AnimCurve c;
    c.Name = name;
    c.Color = color;
    m_Curves.push_back(c);
    return (u32)m_Curves.size() - 1;
}

void CurveEditor::RemoveCurve(u32 index) {
    if (index < m_Curves.size())
        m_Curves.erase(m_Curves.begin() + index);
}

AnimCurve* CurveEditor::GetCurve(u32 index) {
    return (index < m_Curves.size()) ? &m_Curves[index] : nullptr;
}

void CurveEditor::AddKeyframe(u32 curveIdx, f32 time, f32 value, CurveKeyframe::TangentMode mode) {
    if (curveIdx >= m_Curves.size()) return;
    CurveKeyframe k;
    k.Time = time; k.Value = value; k.Mode = mode;

    // 自动切线
    if (mode == CurveKeyframe::Auto) {
        k.InTangent = 0; k.OutTangent = 0;
    }

    auto& keys = m_Curves[curveIdx].Keys;
    keys.push_back(k);
    std::sort(keys.begin(), keys.end(), [](const CurveKeyframe& a, const CurveKeyframe& b) {
        return a.Time < b.Time;
    });

    // Auto-tangent 计算
    for (size_t i = 0; i < keys.size(); i++) {
        if (keys[i].Mode == CurveKeyframe::Auto && keys.size() > 1) {
            f32 prev = (i > 0) ? (keys[i].Value - keys[i-1].Value) / (keys[i].Time - keys[i-1].Time + 0.0001f) : 0;
            f32 next = (i < keys.size()-1) ? (keys[i+1].Value - keys[i].Value) / (keys[i+1].Time - keys[i].Time + 0.0001f) : 0;
            keys[i].InTangent = (prev + next) * 0.5f;
            keys[i].OutTangent = keys[i].InTangent;
        }
    }
}

void CurveEditor::RemoveKeyframe(u32 curveIdx, u32 keyIdx) {
    if (curveIdx >= m_Curves.size()) return;
    auto& keys = m_Curves[curveIdx].Keys;
    if (keyIdx < keys.size())
        keys.erase(keys.begin() + keyIdx);
}

// ── 坐标转换 ────────────────────────────────────────────────

ImVec2 CurveEditor::ToScreen(f32 time, f32 value, ImVec2 cp, ImVec2 cs) const {
    f32 nx = (time - m_TimeMin) / (m_TimeMax - m_TimeMin) * m_ZoomX + m_Pan.x / cs.x;
    f32 ny = 1.0f - (value - m_ValueMin) / (m_ValueMax - m_ValueMin) * m_ZoomY - m_Pan.y / cs.y;
    return ImVec2(cp.x + nx * cs.x, cp.y + ny * cs.y);
}

void CurveEditor::FromScreen(ImVec2 sp, ImVec2 cp, ImVec2 cs, f32& time, f32& value) const {
    f32 nx = (sp.x - cp.x) / cs.x;
    f32 ny = (sp.y - cp.y) / cs.y;
    time  = (nx - m_Pan.x / cs.x) / m_ZoomX * (m_TimeMax - m_TimeMin) + m_TimeMin;
    value = (1.0f - ny - m_Pan.y / cs.y) / m_ZoomY * (m_ValueMax - m_ValueMin) + m_ValueMin;
}

// ── 渲染 ────────────────────────────────────────────────────

void CurveEditor::Render(const char* title) {
    if (!ImGui::Begin(title)) { ImGui::End(); return; }

    RenderToolbar();
    ImGui::Separator();

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    if (canvasSize.x < 50) canvasSize.x = 50;
    if (canvasSize.y < 50) canvasSize.y = 50;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 背景
    dl->AddRectFilled(canvasPos,
                      ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                      IM_COL32(28, 28, 32, 255));

    RenderGrid(dl, canvasPos, canvasSize);
    RenderCurves(dl, canvasPos, canvasSize);
    RenderKeyframes(dl, canvasPos, canvasSize);
    RenderPlayhead(dl, canvasPos, canvasSize);

    // Invisible button 接收输入
    ImGui::SetCursorScreenPos(canvasPos);
    ImGui::InvisibleButton("##CurveCanvas", canvasSize);
    HandleInput(canvasPos, canvasSize);

    ImGui::End();
}

void CurveEditor::RenderToolbar() {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.2f, 1));

    // 曲线列表
    for (size_t i = 0; i < m_Curves.size(); i++) {
        ImVec4 color = ImGui::ColorConvertU32ToFloat4(m_Curves[i].Color);
        if (m_Curves[i].Visible) {
            ImGui::PushStyleColor(ImGuiCol_Text, color);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1));
        }

        bool selected = ((i32)i == m_SelectedCurve);
        if (ImGui::SmallButton(m_Curves[i].Name.c_str())) {
            m_SelectedCurve = (i32)i;
        }
        if (ImGui::IsItemClicked(1)) {
            m_Curves[i].Visible = !m_Curves[i].Visible;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    ImGui::Text("|");
    ImGui::SameLine();
    ImGui::Text("T: %.2f", m_PlayheadTime);

    ImGui::PopStyleColor();
}

void CurveEditor::RenderGrid(ImDrawList* dl, ImVec2 cp, ImVec2 cs) {
    // 垂直线 (时间)
    int numVLines = 10;
    for (int i = 0; i <= numVLines; i++) {
        f32 t = m_TimeMin + (m_TimeMax - m_TimeMin) * i / numVLines;
        ImVec2 p = ToScreen(t, 0, cp, cs);
        dl->AddLine(ImVec2(p.x, cp.y), ImVec2(p.x, cp.y + cs.y),
                    IM_COL32(50, 50, 55, 150));

        char label[32];
        snprintf(label, sizeof(label), "%.1f", t);
        dl->AddText(ImVec2(p.x + 2, cp.y + cs.y - 14), IM_COL32(120, 120, 140, 200), label);
    }

    // 水平线 (值)
    int numHLines = 6;
    for (int i = 0; i <= numHLines; i++) {
        f32 v = m_ValueMin + (m_ValueMax - m_ValueMin) * i / numHLines;
        ImVec2 p = ToScreen(0, v, cp, cs);
        dl->AddLine(ImVec2(cp.x, p.y), ImVec2(cp.x + cs.x, p.y),
                    IM_COL32(50, 50, 55, 150));

        char label[32];
        snprintf(label, sizeof(label), "%.2f", v);
        dl->AddText(ImVec2(cp.x + 2, p.y - 12), IM_COL32(120, 120, 140, 200), label);
    }

    // 零线高亮
    ImVec2 zeroY = ToScreen(0, 0, cp, cs);
    if (zeroY.y >= cp.y && zeroY.y <= cp.y + cs.y)
        dl->AddLine(ImVec2(cp.x, zeroY.y), ImVec2(cp.x + cs.x, zeroY.y),
                    IM_COL32(80, 80, 90, 200), 1.5f);
}

void CurveEditor::RenderCurves(ImDrawList* dl, ImVec2 cp, ImVec2 cs) {
    for (auto& curve : m_Curves) {
        if (!curve.Visible || curve.Keys.size() < 2) continue;

        // 采样绘制
        int samples = (int)cs.x;
        ImVec2 prevPt = {0, 0};
        for (int s = 0; s <= samples; s++) {
            f32 t = m_TimeMin + (m_TimeMax - m_TimeMin) * s / samples;
            f32 v = curve.Evaluate(t);
            ImVec2 pt = ToScreen(t, v, cp, cs);

            if (s > 0) {
                dl->AddLine(prevPt, pt, curve.Color, 2.0f);
            }
            prevPt = pt;
        }
    }
}

void CurveEditor::RenderKeyframes(ImDrawList* dl, ImVec2 cp, ImVec2 cs) {
    float keyRadius = 5.0f;
    float tangentLen = 40.0f;

    for (size_t ci = 0; ci < m_Curves.size(); ci++) {
        auto& curve = m_Curves[ci];
        if (!curve.Visible) continue;

        for (size_t ki = 0; ki < curve.Keys.size(); ki++) {
            auto& key = curve.Keys[ki];
            ImVec2 pos = ToScreen(key.Time, key.Value, cp, cs);

            bool isSelected = ((i32)ci == m_SelectedCurve && (i32)ki == m_SelectedKey);

            // 关键帧菱形
            ImU32 keyColor = isSelected ? IM_COL32(255, 255, 100, 255) : curve.Color;
            ImVec2 diamond[4] = {
                {pos.x, pos.y - keyRadius},
                {pos.x + keyRadius, pos.y},
                {pos.x, pos.y + keyRadius},
                {pos.x - keyRadius, pos.y},
            };
            dl->AddConvexPolyFilled(diamond, 4, keyColor);
            dl->AddPolyline(diamond, 4, IM_COL32(0,0,0,150), true, 1.0f);

            // 切线手柄 (选中时)
            if (isSelected && key.Mode != CurveKeyframe::Constant) {
                // 入切线
                ImVec2 inHandle(pos.x - tangentLen, pos.y + tangentLen * key.InTangent);
                dl->AddLine(pos, inHandle, IM_COL32(200, 200, 200, 150), 1.0f);
                dl->AddCircleFilled(inHandle, 3.5f, IM_COL32(200, 100, 100, 255));

                // 出切线
                ImVec2 outHandle(pos.x + tangentLen, pos.y - tangentLen * key.OutTangent);
                dl->AddLine(pos, outHandle, IM_COL32(200, 200, 200, 150), 1.0f);
                dl->AddCircleFilled(outHandle, 3.5f, IM_COL32(100, 200, 100, 255));
            }
        }
    }
}

void CurveEditor::RenderPlayhead(ImDrawList* dl, ImVec2 cp, ImVec2 cs) {
    ImVec2 phPos = ToScreen(m_PlayheadTime, 0, cp, cs);
    if (phPos.x >= cp.x && phPos.x <= cp.x + cs.x) {
        dl->AddLine(ImVec2(phPos.x, cp.y), ImVec2(phPos.x, cp.y + cs.y),
                    IM_COL32(255, 80, 80, 200), 1.5f);

        // 播放头三角形
        ImVec2 tri[3] = {
            {phPos.x - 6, cp.y},
            {phPos.x + 6, cp.y},
            {phPos.x, cp.y + 10},
        };
        dl->AddTriangleFilled(tri[0], tri[1], tri[2], IM_COL32(255, 80, 80, 220));
    }
}

void CurveEditor::HandleInput(ImVec2 cp, ImVec2 cs) {
    ImGuiIO& io = ImGui::GetIO();

    if (!ImGui::IsItemHovered()) return;

    // 中键拖拽平移
    if (ImGui::IsMouseDragging(2)) {
        m_Pan.x += io.MouseDelta.x;
        m_Pan.y += io.MouseDelta.y;
    }

    // 缩放
    if (io.MouseWheel != 0) {
        float factor = 1.0f + io.MouseWheel * 0.1f;
        m_ZoomX *= factor;
        m_ZoomY *= factor;
        m_ZoomX = std::clamp(m_ZoomX, 0.1f, 10.0f);
        m_ZoomY = std::clamp(m_ZoomY, 0.1f, 10.0f);
    }

    // 点击选中关键帧
    if (ImGui::IsMouseClicked(0)) {
        float bestDist = 15.0f;
        m_SelectedKey = -1;

        for (size_t ci = 0; ci < m_Curves.size(); ci++) {
            auto& curve = m_Curves[ci];
            if (!curve.Visible) continue;
            for (size_t ki = 0; ki < curve.Keys.size(); ki++) {
                ImVec2 kPos = ToScreen(curve.Keys[ki].Time, curve.Keys[ki].Value, cp, cs);
                float dx = io.MousePos.x - kPos.x;
                float dy = io.MousePos.y - kPos.y;
                float dist = std::sqrt(dx*dx + dy*dy);
                if (dist < bestDist) {
                    bestDist = dist;
                    m_SelectedCurve = (i32)ci;
                    m_SelectedKey = (i32)ki;
                    m_DraggingKey = true;
                }
            }
        }
    }

    // 拖拽关键帧
    if (m_DraggingKey && m_SelectedCurve >= 0 && m_SelectedKey >= 0 && ImGui::IsMouseDragging(0)) {
        f32 time, value;
        FromScreen(io.MousePos, cp, cs, time, value);
        auto& key = m_Curves[m_SelectedCurve].Keys[m_SelectedKey];
        key.Time = time;
        key.Value = value;
    }

    if (ImGui::IsMouseReleased(0)) {
        m_DraggingKey = false;
        m_DraggingTangent = false;
        // 排序关键帧
        if (m_SelectedCurve >= 0 && (u32)m_SelectedCurve < m_Curves.size()) {
            auto& keys = m_Curves[m_SelectedCurve].Keys;
            std::sort(keys.begin(), keys.end(), [](const CurveKeyframe& a, const CurveKeyframe& b) {
                return a.Time < b.Time;
            });
        }
    }

    // 右键添加关键帧
    if (ImGui::IsMouseClicked(1)) {
        if (m_SelectedCurve >= 0 && (u32)m_SelectedCurve < m_Curves.size()) {
            f32 time, value;
            FromScreen(io.MousePos, cp, cs, time, value);
            AddKeyframe((u32)m_SelectedCurve, time, value);
        }
    }

    // Delete 键
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && m_SelectedKey >= 0 && m_SelectedCurve >= 0) {
        RemoveKeyframe((u32)m_SelectedCurve, (u32)m_SelectedKey);
        m_SelectedKey = -1;
    }
}

} // namespace Engine
