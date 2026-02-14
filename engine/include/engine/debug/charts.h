#pragma once

#include "engine/core/types.h"

#include <imgui.h>
#include <string>
#include <vector>

namespace Engine {

// ── 数据图表框架 ────────────────────────────────────────────
// ImGui 绘制的实时图表, 集成到 Profiler 面板
// 支持: 折线图 / 柱状图 / 热力图 / 饼图 / 时间线

class Charts {
public:
    // ── 折线图 ──────────────────────────────────────
    struct LineChartData {
        std::string Label;
        std::vector<f32> Values;
        ImVec4 Color = {1, 1, 1, 1};
        f32 MinY = 0, MaxY = 0;
    };

    static void LineChart(const char* title,
                          const std::vector<LineChartData>& series,
                          ImVec2 size = {0, 100});

    // ── 柱状图 ──────────────────────────────────────
    struct BarChartEntry {
        std::string Label;
        f32 Value = 0;
        ImVec4 Color = {0.3f, 0.6f, 1.0f, 1.0f};
    };

    static void BarChart(const char* title,
                         const std::vector<BarChartEntry>& entries,
                         ImVec2 size = {0, 120});

    // ── 饼图 ────────────────────────────────────────
    struct PieSlice {
        std::string Label;
        f32 Value = 0;
        ImVec4 Color;
    };

    static void PieChart(const char* title,
                         const std::vector<PieSlice>& slices,
                         f32 radius = 50.0f);

    // ── 热力图 ──────────────────────────────────────
    static void Heatmap(const char* title,
                        const f32* data, u32 rows, u32 cols,
                        f32 minVal, f32 maxVal,
                        ImVec2 cellSize = {12, 12});

    // ── 时间线 ──────────────────────────────────────
    struct TimelineEntry {
        std::string Label;
        f32 Start;
        f32 End;
        ImVec4 Color;
    };

    static void Timeline(const char* title,
                         const std::vector<TimelineEntry>& entries,
                         f32 totalDuration,
                         ImVec2 size = {0, 80});
};

} // namespace Engine
