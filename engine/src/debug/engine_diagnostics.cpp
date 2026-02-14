#include "engine/debug/engine_diagnostics.h"
#include "engine/core/log.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <functional>

namespace Engine {

void EngineDiagnostics::Init() {
    LOG_INFO("[Insight] 初始化引擎诊断工具");
}

void EngineDiagnostics::Shutdown() {
    s_RenderTargets.clear();
    s_FlameEntries.clear();
    s_Textures.clear();
    s_DrawCallGroups.clear();
    s_FrameHistory.clear();
    LOG_INFO("[Insight] 关闭");
}

void EngineDiagnostics::Render() {
    if (s_ShowRenderTargets)    RenderRenderTargets();
    if (s_ShowFlameGraph)       RenderFlameGraph();
    if (s_ShowTextureBrowser)   RenderTextureBrowser();
    if (s_ShowDrawCallAnalysis) RenderDrawCallAnalysis();
    if (s_ShowFrameHistory)     RenderFrameHistory();
}

void EngineDiagnostics::ToggleRenderTargets()    { s_ShowRenderTargets = !s_ShowRenderTargets; }
void EngineDiagnostics::ToggleFlameGraph()       { s_ShowFlameGraph = !s_ShowFlameGraph; }
void EngineDiagnostics::ToggleTextureBrowser()   { s_ShowTextureBrowser = !s_ShowTextureBrowser; }
void EngineDiagnostics::ToggleDrawCallAnalysis() { s_ShowDrawCallAnalysis = !s_ShowDrawCallAnalysis; }
void EngineDiagnostics::ToggleFrameHistory()     { s_ShowFrameHistory = !s_ShowFrameHistory; }

// ── 数据录入 ────────────────────────────────────────────────

void EngineDiagnostics::RegisterRenderTarget(const std::string& name, u32 texID,
                                              u32 w, u32 h, const std::string& format) {
    for (auto& rt : s_RenderTargets) {
        if (rt.Name == name) { rt.TextureID = texID; rt.Width = w; rt.Height = h; rt.Format = format; return; }
    }
    s_RenderTargets.push_back({name, texID, w, h, format});
}

void EngineDiagnostics::ClearRenderTargets() { s_RenderTargets.clear(); }

void EngineDiagnostics::RecordFlameEntry(const std::string& name, f32 startMs,
                                          f32 durationMs, u32 depth) {
    s_FlameEntries.push_back({name, startMs, durationMs, depth, {}});
}

void EngineDiagnostics::ClearFlameEntries() { s_FlameEntries.clear(); }

void EngineDiagnostics::RegisterTexture(const std::string& name, u32 texID,
                                         u32 w, u32 h, size_t vram,
                                         const std::string& format) {
    for (auto& t : s_Textures) {
        if (t.Name == name) { t.TextureID = texID; t.Width = w; t.Height = h; t.VRAMBytes = vram; t.Format = format; return; }
    }
    s_Textures.push_back({name, texID, w, h, vram, format});
}

void EngineDiagnostics::ClearTextures() { s_Textures.clear(); }

void EngineDiagnostics::RecordDrawCallGroup(const std::string& shader,
                                             const std::string& material,
                                             u32 draws, u32 tris, f32 gpuMs) {
    s_DrawCallGroups.push_back({shader, material, draws, tris, gpuMs});
}

void EngineDiagnostics::ClearDrawCallGroups() { s_DrawCallGroups.clear(); }

void EngineDiagnostics::PushFrameSnapshot(const FrameSnapshot& snapshot) {
    if (s_HistoryPaused) return;
    s_FrameHistory.push_back(snapshot);
    if (s_FrameHistory.size() > MAX_FRAME_HISTORY)
        s_FrameHistory.erase(s_FrameHistory.begin());
}

void EngineDiagnostics::SetFrameHistoryPaused(bool paused) { s_HistoryPaused = paused; }
bool EngineDiagnostics::IsFrameHistoryPaused() { return s_HistoryPaused; }

// ── 渲染目标浏览器 ──────────────────────────────────────────

void EngineDiagnostics::RenderRenderTargets() {
    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("渲染目标浏览器", &s_ShowRenderTargets)) { ImGui::End(); return; }

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1), "共 %zu 个渲染目标", s_RenderTargets.size());
    ImGui::Separator();

    // 网格布局预览
    float thumbSize = 160.0f;
    float windowW = ImGui::GetContentRegionAvail().x;
    int cols = std::max(1, (int)(windowW / (thumbSize + 10)));

    int col = 0;
    for (auto& rt : s_RenderTargets) {
        ImGui::BeginGroup();

        // 缩略图
        if (rt.TextureID > 0) {
            ImGui::Image((ImTextureID)(intptr_t)rt.TextureID,
                         ImVec2(thumbSize, thumbSize * 0.5625f));  // 16:9
        } else {
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                pos, ImVec2(pos.x + thumbSize, pos.y + thumbSize * 0.5625f),
                IM_COL32(40, 40, 40, 255));
            ImGui::Dummy(ImVec2(thumbSize, thumbSize * 0.5625f));
        }

        // 标签
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1), "%s", rt.Name.c_str());
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "%ux%u %s",
                          rt.Width, rt.Height, rt.Format.c_str());

        ImGui::EndGroup();

        col++;
        if (col < cols) ImGui::SameLine();
        else col = 0;
    }

    ImGui::End();
}

// ── CPU/GPU 火焰图 ──────────────────────────────────────────

ImU32 EngineDiagnostics::GetFlameColor(u32 depth, const std::string& name) {
    // 基于名称 hash 生成颜色 (确保同名 Pass 颜色一致)
    u32 hash = 5381;
    for (char c : name) hash = ((hash << 5) + hash) + c;

    float hue = (float)(hash % 360) / 360.0f;
    float sat = 0.6f + (depth * 0.05f);
    if (sat > 0.9f) sat = 0.9f;
    float val = 0.8f - (depth * 0.1f);
    if (val < 0.4f) val = 0.4f;

    // HSV → RGB
    ImVec4 rgb;
    ImGui::ColorConvertHSVtoRGB(hue, sat, val, rgb.x, rgb.y, rgb.z);
    return IM_COL32((u8)(rgb.x*255), (u8)(rgb.y*255), (u8)(rgb.z*255), 220);
}

void EngineDiagnostics::RenderFlameGraph() {
    ImGui::SetNextWindowSize(ImVec2(800, 300), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("火焰图 (CPU + GPU)", &s_ShowFlameGraph)) { ImGui::End(); return; }

    if (s_FlameEntries.empty()) {
        ImGui::Text("无火焰图数据");
        ImGui::End();
        return;
    }

    // 总时间范围
    f32 maxTime = 0;
    u32 maxDepth = 0;
    for (auto& e : s_FlameEntries) {
        f32 end = e.StartMs + e.DurationMs;
        if (end > maxTime) maxTime = end;
        if (e.Depth > maxDepth) maxDepth = e.Depth;
    }
    if (maxTime < 0.001f) maxTime = 16.7f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    canvasSize.y = std::max(canvasSize.y, (float)(maxDepth + 1) * 24.0f + 30.0f);

    // 背景
    dl->AddRectFilled(canvasPos,
                      ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                      IM_COL32(25, 25, 30, 255));

    // 时间刻度线
    for (int i = 0; i <= 4; i++) {
        f32 t = maxTime * (f32)i / 4.0f;
        f32 x = canvasPos.x + (t / maxTime) * canvasSize.x;
        dl->AddLine(ImVec2(x, canvasPos.y),
                    ImVec2(x, canvasPos.y + canvasSize.y),
                    IM_COL32(60, 60, 60, 150));
        char label[16]; snprintf(label, 16, "%.1fms", t);
        dl->AddText(ImVec2(x + 2, canvasPos.y + 2), IM_COL32(150,150,150,200), label);
    }

    // 16.7ms 参考线 (60fps)
    if (maxTime > 16.7f) {
        f32 x60 = canvasPos.x + (16.7f / maxTime) * canvasSize.x;
        dl->AddLine(ImVec2(x60, canvasPos.y),
                    ImVec2(x60, canvasPos.y + canvasSize.y),
                    IM_COL32(0, 200, 0, 100), 2.0f);
    }

    // 绘制火焰条
    float barH = 20.0f;
    float topOffset = 20.0f;

    for (auto& entry : s_FlameEntries) {
        f32 x0 = canvasPos.x + (entry.StartMs / maxTime) * canvasSize.x;
        f32 x1 = canvasPos.x + ((entry.StartMs + entry.DurationMs) / maxTime) * canvasSize.x;
        f32 y0 = canvasPos.y + topOffset + entry.Depth * (barH + 2);
        f32 y1 = y0 + barH;

        if (x1 - x0 < 1.0f) x1 = x0 + 1.0f;

        ImU32 color = GetFlameColor(entry.Depth, entry.Name);
        dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), color, 2.0f);
        dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(0,0,0,80), 2.0f);

        // 标签 (足够宽时)
        if (x1 - x0 > 40) {
            char label[64];
            snprintf(label, 64, "%s %.2fms", entry.Name.c_str(), entry.DurationMs);
            dl->AddText(ImVec2(x0 + 3, y0 + 3), IM_COL32(255,255,255,220), label);
        }

        // Tooltip
        ImVec2 mousePos = ImGui::GetIO().MousePos;
        if (mousePos.x >= x0 && mousePos.x <= x1 && mousePos.y >= y0 && mousePos.y <= y1) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", entry.Name.c_str());
            ImGui::Text("耗时: %.3f ms", entry.DurationMs);
            ImGui::Text("开始: %.3f ms", entry.StartMs);
            ImGui::Text("深度: %u", entry.Depth);
            ImGui::EndTooltip();
        }
    }

    ImGui::Dummy(canvasSize);
    ImGui::End();
}

// ── 纹理浏览器 ──────────────────────────────────────────────

void EngineDiagnostics::RenderTextureBrowser() {
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("纹理浏览器", &s_ShowTextureBrowser)) { ImGui::End(); return; }

    // 统计
    size_t totalVRAM = 0;
    for (auto& t : s_Textures) totalVRAM += t.VRAMBytes;
    float totalMB = (float)totalVRAM / (1024.0f * 1024.0f);

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1), "共 %zu 纹理 | VRAM: %.1f MB",
                      s_Textures.size(), totalMB);
    ImGui::Separator();

    // 排序: VRAM 从大到小
    auto sorted = s_Textures;
    std::sort(sorted.begin(), sorted.end(),
              [](const TextureInfo& a, const TextureInfo& b) { return a.VRAMBytes > b.VRAMBytes; });

    // 表格
    if (ImGui::BeginTable("##TexTable", 5,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_Sortable | ImGuiTableFlags_Resizable)) {

        ImGui::TableSetupColumn("预览", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("尺寸", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("格式", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("VRAM", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();

        for (auto& tex : sorted) {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            if (tex.TextureID > 0) {
                ImGui::Image((ImTextureID)(intptr_t)tex.TextureID, ImVec2(48, 48));
            }

            ImGui::TableNextColumn();
            ImGui::Text("%s", tex.Name.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%ux%u", tex.Width, tex.Height);

            ImGui::TableNextColumn();
            ImGui::Text("%s", tex.Format.c_str());

            ImGui::TableNextColumn();
            float mb = (float)tex.VRAMBytes / (1024.0f * 1024.0f);
            ImVec4 vramColor = (mb > 4) ? ImVec4(1,0.3f,0.3f,1) :
                               (mb > 1) ? ImVec4(1,0.8f,0.2f,1) :
                                           ImVec4(0.5f,0.9f,0.5f,1);
            ImGui::TextColored(vramColor, "%.1f MB", mb);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

// ── DrawCall 分析 ───────────────────────────────────────────

void EngineDiagnostics::RenderDrawCallAnalysis() {
    ImGui::SetNextWindowSize(ImVec2(600, 350), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("DrawCall 分析", &s_ShowDrawCallAnalysis)) { ImGui::End(); return; }

    if (s_DrawCallGroups.empty()) {
        ImGui::Text("无 DrawCall 数据");
        ImGui::End();
        return;
    }

    // 排序: GPU 耗时从大到小
    auto sorted = s_DrawCallGroups;
    std::sort(sorted.begin(), sorted.end(),
              [](const DrawCallGroup& a, const DrawCallGroup& b) { return a.GPUTimeMs > b.GPUTimeMs; });

    u32 totalDraws = 0, totalTris = 0;
    f32 totalGPU = 0;
    for (auto& g : sorted) { totalDraws += g.DrawCalls; totalTris += g.Triangles; totalGPU += g.GPUTimeMs; }

    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1),
                      "总计: %u DC | %u Tri | %.2f ms GPU", totalDraws, totalTris, totalGPU);
    ImGui::Separator();

    if (ImGui::BeginTable("##DCTable", 5,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {

        ImGui::TableSetupColumn("Shader");
        ImGui::TableSetupColumn("Material");
        ImGui::TableSetupColumn("DC");
        ImGui::TableSetupColumn("Triangles");
        ImGui::TableSetupColumn("GPU ms");
        ImGui::TableHeadersRow();

        for (auto& g : sorted) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%s", g.ShaderName.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%s", g.MaterialName.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%u", g.DrawCalls);
            ImGui::TableNextColumn(); ImGui::Text("%u", g.Triangles);
            ImGui::TableNextColumn();
            f32 pct = (totalGPU > 0) ? (g.GPUTimeMs / totalGPU * 100.0f) : 0;
            ImVec4 color = (pct > 30) ? ImVec4(1,0.3f,0.3f,1) : ImVec4(0.7f,0.7f,0.7f,1);
            ImGui::TextColored(color, "%.2f (%.0f%%)", g.GPUTimeMs, pct);
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

// ── 帧历史 + 回溯滑块 ──────────────────────────────────────

void EngineDiagnostics::RenderFrameHistory() {
    ImGui::SetNextWindowSize(ImVec2(800, 200), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("帧历史", &s_ShowFrameHistory)) { ImGui::End(); return; }

    if (s_FrameHistory.empty()) {
        ImGui::Text("无帧历史数据");
        ImGui::End();
        return;
    }

    // 暂停按钮
    if (ImGui::Button(s_HistoryPaused ? "▶ 继续" : "⏸ 暂停")) {
        s_HistoryPaused = !s_HistoryPaused;
    }

    ImGui::SameLine();
    ImGui::Text("帧数: %zu / %u", s_FrameHistory.size(), MAX_FRAME_HISTORY);

    // 帧时间柱状图
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    float height = 80.0f;

    dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
                      IM_COL32(25, 25, 30, 255));

    // 16.7ms 参考线
    float refY = pos.y + height - (16.7f / 50.0f) * height;
    dl->AddLine(ImVec2(pos.x, refY), ImVec2(pos.x + width, refY),
                IM_COL32(0, 200, 0, 80));

    float barW = width / (float)s_FrameHistory.size();

    for (size_t i = 0; i < s_FrameHistory.size(); i++) {
        auto& frame = s_FrameHistory[i];
        float h = (frame.TotalMs / 50.0f) * height;
        if (h > height) h = height;

        float x = pos.x + (float)i * barW;
        float y = pos.y + height - h;

        ImU32 barColor;
        if (frame.TotalMs > 33.3f)
            barColor = IM_COL32(255, 60, 60, 200);
        else if (frame.TotalMs > 16.7f)
            barColor = IM_COL32(255, 200, 60, 200);
        else
            barColor = IM_COL32(60, 180, 60, 200);

        bool selected = (s_SelectedFrame == (i32)i);
        if (selected) barColor = IM_COL32(100, 150, 255, 255);

        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + barW - 1, pos.y + height), barColor);

        // 点击选择
        ImVec2 mousePos = ImGui::GetIO().MousePos;
        if (mousePos.x >= x && mousePos.x < x + barW &&
            mousePos.y >= pos.y && mousePos.y <= pos.y + height) {
            ImGui::BeginTooltip();
            ImGui::Text("帧 #%zu: %.2f ms | DC: %u | Tri: %u",
                        i, frame.TotalMs, frame.DrawCalls, frame.Triangles);
            ImGui::EndTooltip();

            if (ImGui::IsMouseClicked(0)) {
                s_SelectedFrame = (i32)i;
            }
        }
    }

    ImGui::Dummy(ImVec2(width, height));

    // 选中帧详情
    if (s_SelectedFrame >= 0 && s_SelectedFrame < (i32)s_FrameHistory.size()) {
        auto& frame = s_FrameHistory[s_SelectedFrame];
        ImGui::Separator();
        ImGui::Text("选中帧 #%d: Total=%.2f ms | CPU=%.2f ms | GPU=%.2f ms | DC=%u | Tri=%u",
                    s_SelectedFrame, frame.TotalMs, frame.CPUMs, frame.GPUMs,
                    frame.DrawCalls, frame.Triangles);
    }

    ImGui::End();
}

} // namespace Engine
