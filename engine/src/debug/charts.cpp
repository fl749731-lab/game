#include "engine/debug/charts.h"

#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Engine {

// ── 折线图 ──────────────────────────────────────────────────

void Charts::LineChart(const char* title,
                       const std::vector<LineChartData>& series,
                       ImVec2 size) {
    ImGui::Text("%s", title);
    if (size.x <= 0) size.x = ImGui::GetContentRegionAvail().x;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 背景
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                      IM_COL32(30, 30, 30, 200));

    for (auto& s : series) {
        if (s.Values.size() < 2) continue;
        f32 minY = s.MinY, maxY = s.MaxY;
        if (minY == maxY) {
            auto [mi, ma] = std::minmax_element(s.Values.begin(), s.Values.end());
            minY = *mi; maxY = *ma;
            if (minY == maxY) { maxY += 1.0f; }
        }
        f32 rangeY = maxY - minY;

        for (size_t i = 1; i < s.Values.size(); i++) {
            float t0 = (float)(i-1) / (float)(s.Values.size()-1);
            float t1 = (float)i / (float)(s.Values.size()-1);
            float y0 = 1.0f - (s.Values[i-1] - minY) / rangeY;
            float y1 = 1.0f - (s.Values[i] - minY) / rangeY;

            ImVec2 p0(pos.x + t0 * size.x, pos.y + y0 * size.y);
            ImVec2 p1(pos.x + t1 * size.x, pos.y + y1 * size.y);
            dl->AddLine(p0, p1, ImGui::ColorConvertFloat4ToU32(s.Color), 1.5f);
        }
    }

    ImGui::Dummy(size);
}

// ── 柱状图 ──────────────────────────────────────────────────

void Charts::BarChart(const char* title,
                      const std::vector<BarChartEntry>& entries,
                      ImVec2 size) {
    ImGui::Text("%s", title);
    if (entries.empty()) return;
    if (size.x <= 0) size.x = ImGui::GetContentRegionAvail().x;

    f32 maxVal = 0;
    for (auto& e : entries) maxVal = std::max(maxVal, e.Value);
    if (maxVal < 1e-6f) maxVal = 1.0f;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                      IM_COL32(30, 30, 30, 200));

    float barWidth = size.x / (float)entries.size() * 0.7f;
    float gap = size.x / (float)entries.size() * 0.3f;

    for (size_t i = 0; i < entries.size(); i++) {
        float h = (entries[i].Value / maxVal) * size.y * 0.9f;
        float x = pos.x + (float)i * (barWidth + gap) + gap * 0.5f;
        float y = pos.y + size.y - h;

        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + barWidth, pos.y + size.y),
                          ImGui::ColorConvertFloat4ToU32(entries[i].Color));

        dl->AddText(ImVec2(x, pos.y + size.y + 2), IM_COL32(200,200,200,255),
                    entries[i].Label.c_str());
    }

    ImGui::Dummy(ImVec2(size.x, size.y + 16));
}

// ── 饼图 ────────────────────────────────────────────────────

void Charts::PieChart(const char* title,
                      const std::vector<PieSlice>& slices,
                      f32 radius) {
    ImGui::Text("%s", title);
    if (slices.empty()) return;

    f32 total = 0;
    for (auto& s : slices) total += s.Value;
    if (total < 1e-6f) return;

    ImVec2 center = ImGui::GetCursorScreenPos();
    center.x += radius + 10;
    center.y += radius + 10;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    f32 startAngle = -(f32)M_PI / 2.0f;

    for (auto& slice : slices) {
        f32 sweepAngle = (slice.Value / total) * 2.0f * (f32)M_PI;
        ImU32 col = ImGui::ColorConvertFloat4ToU32(slice.Color);

        // 画扇形 (三角形扇)
        constexpr int segments = 32;
        for (int j = 0; j < segments; j++) {
            f32 a0 = startAngle + sweepAngle * (f32)j / (f32)segments;
            f32 a1 = startAngle + sweepAngle * (f32)(j+1) / (f32)segments;
            dl->AddTriangleFilled(
                center,
                ImVec2(center.x + std::cos(a0) * radius, center.y + std::sin(a0) * radius),
                ImVec2(center.x + std::cos(a1) * radius, center.y + std::sin(a1) * radius),
                col);
        }

        // 标签
        f32 midAngle = startAngle + sweepAngle * 0.5f;
        f32 labelR = radius * 0.65f;
        ImVec2 labelPos(center.x + std::cos(midAngle) * labelR - 10,
                        center.y + std::sin(midAngle) * labelR - 5);
        dl->AddText(labelPos, IM_COL32(255,255,255,255), slice.Label.c_str());

        startAngle += sweepAngle;
    }

    ImGui::Dummy(ImVec2(radius * 2 + 20, radius * 2 + 20));
}

// ── 热力图 ──────────────────────────────────────────────────

void Charts::Heatmap(const char* title,
                     const f32* data, u32 rows, u32 cols,
                     f32 minVal, f32 maxVal,
                     ImVec2 cellSize) {
    ImGui::Text("%s", title);
    if (!data || rows == 0 || cols == 0) return;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    f32 range = maxVal - minVal;
    if (range < 1e-6f) range = 1.0f;

    for (u32 r = 0; r < rows; r++) {
        for (u32 c = 0; c < cols; c++) {
            f32 val = data[r * cols + c];
            f32 t = (val - minVal) / range;
            t = std::clamp(t, 0.0f, 1.0f);

            // 蓝→绿→红渐变
            ImVec4 color;
            if (t < 0.5f) {
                float tt = t / 0.5f;
                color = ImVec4(0, tt, 1.0f - tt, 1);
            } else {
                float tt = (t - 0.5f) / 0.5f;
                color = ImVec4(tt, 1.0f - tt, 0, 1);
            }

            ImVec2 p0(pos.x + c * cellSize.x, pos.y + r * cellSize.y);
            ImVec2 p1(p0.x + cellSize.x - 1, p0.y + cellSize.y - 1);
            dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(color));
        }
    }

    ImGui::Dummy(ImVec2(cols * cellSize.x, rows * cellSize.y));
}

// ── 时间线 ──────────────────────────────────────────────────

void Charts::Timeline(const char* title,
                      const std::vector<TimelineEntry>& entries,
                      f32 totalDuration,
                      ImVec2 size) {
    ImGui::Text("%s", title);
    if (entries.empty() || totalDuration <= 0) return;
    if (size.x <= 0) size.x = ImGui::GetContentRegionAvail().x;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                      IM_COL32(25, 25, 25, 200));

    f32 barH = size.y / (f32)std::max((size_t)1, entries.size());

    for (size_t i = 0; i < entries.size(); i++) {
        auto& e = entries[i];
        float x0 = pos.x + (e.Start / totalDuration) * size.x;
        float x1 = pos.x + (e.End / totalDuration) * size.x;
        float y0 = pos.y + (float)i * barH;
        float y1 = y0 + barH - 1;

        dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1),
                          ImGui::ColorConvertFloat4ToU32(e.Color));
        dl->AddText(ImVec2(x0 + 2, y0 + 1), IM_COL32(255,255,255,255),
                    e.Label.c_str());
    }

    ImGui::Dummy(size);
}

} // namespace Engine
