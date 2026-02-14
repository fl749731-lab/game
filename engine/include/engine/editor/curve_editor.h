#pragma once

#include "engine/core/types.h"

#include <imgui.h>
#include <string>
#include <vector>

namespace Engine {

// ── 关键帧 ──────────────────────────────────────────────────

struct CurveKeyframe {
    f32 Time  = 0;   // X 轴
    f32 Value = 0;   // Y 轴
    f32 InTangent  = 0;  // 入切线斜率
    f32 OutTangent = 0;  // 出切线斜率

    // 切线模式
    enum TangentMode : u8 { Free, Linear, Constant, Auto };
    TangentMode Mode = Auto;
};

// ── 曲线通道 ────────────────────────────────────────────────

struct AnimCurve {
    std::string Name;
    ImU32 Color = IM_COL32(255, 200, 80, 255);
    std::vector<CurveKeyframe> Keys;
    bool Visible = true;
    bool Locked  = false;

    /// Hermite 插值求值
    f32 Evaluate(f32 time) const;
};

// ── 曲线编辑器面板 ──────────────────────────────────────────
// 功能:
//   1. 多通道贝塞尔/Hermite 曲线显示
//   2. 关键帧拖拽 (位置 + 切线手柄)
//   3. 框选关键帧
//   4. 右键添加/删除关键帧
//   5. 时间轴刻度 + 网格
//   6. 缩放/平移
//   7. 播放头指示器

class CurveEditor {
public:
    CurveEditor();
    ~CurveEditor();

    void Render(const char* title = "曲线编辑器");

    // 曲线管理
    u32  AddCurve(const std::string& name, ImU32 color = IM_COL32(255, 200, 80, 255));
    void RemoveCurve(u32 index);
    AnimCurve* GetCurve(u32 index);

    // 关键帧操作
    void AddKeyframe(u32 curveIdx, f32 time, f32 value, CurveKeyframe::TangentMode mode = CurveKeyframe::Auto);
    void RemoveKeyframe(u32 curveIdx, u32 keyIdx);

    // 播放头
    void SetPlayheadTime(f32 time) { m_PlayheadTime = time; }
    f32  GetPlayheadTime() const { return m_PlayheadTime; }

    // 范围
    void SetTimeRange(f32 min, f32 max) { m_TimeMin = min; m_TimeMax = max; }
    void SetValueRange(f32 min, f32 max) { m_ValueMin = min; m_ValueMax = max; }

private:
    void RenderGrid(ImDrawList* dl, ImVec2 canvasPos, ImVec2 canvasSize);
    void RenderCurves(ImDrawList* dl, ImVec2 canvasPos, ImVec2 canvasSize);
    void RenderKeyframes(ImDrawList* dl, ImVec2 canvasPos, ImVec2 canvasSize);
    void RenderPlayhead(ImDrawList* dl, ImVec2 canvasPos, ImVec2 canvasSize);
    void RenderToolbar();
    void HandleInput(ImVec2 canvasPos, ImVec2 canvasSize);

    // 坐标转换
    ImVec2 ToScreen(f32 time, f32 value, ImVec2 canvasPos, ImVec2 canvasSize) const;
    void FromScreen(ImVec2 screenPos, ImVec2 canvasPos, ImVec2 canvasSize, f32& time, f32& value) const;

    std::vector<AnimCurve> m_Curves;
    f32 m_TimeMin = 0, m_TimeMax = 5.0f;
    f32 m_ValueMin = -1.0f, m_ValueMax = 1.0f;
    f32 m_PlayheadTime = 0;

    // 选中状态
    i32 m_SelectedCurve = -1;
    i32 m_SelectedKey = -1;
    bool m_DraggingKey = false;
    bool m_DraggingTangent = false;
    bool m_DraggingInTangent = false;

    // 平移/缩放
    ImVec2 m_Pan = {0, 0};
    float  m_ZoomX = 1.0f, m_ZoomY = 1.0f;
};

} // namespace Engine
