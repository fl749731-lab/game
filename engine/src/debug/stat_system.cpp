#include "engine/debug/stat_system.h"
#include "engine/core/log.h"

#include <imgui.h>
#include <cstdio>
#include <algorithm>

namespace Engine {

void StatOverlay::Init() {
    s_ActiveCategories = StatCategory::None;
    s_GPUPasses.clear();
    s_MemoryEntries.clear();
    LOG_INFO("[StatOverlay] 初始化");
}

void StatOverlay::Shutdown() {
    LOG_INFO("[StatOverlay] 关闭");
}

void StatOverlay::Toggle(StatCategory cat) {
    s_ActiveCategories = (StatCategory)((u8)s_ActiveCategories ^ (u8)cat);
}

void StatOverlay::Enable(StatCategory cat) {
    s_ActiveCategories = s_ActiveCategories | cat;
}

void StatOverlay::Disable(StatCategory cat) {
    s_ActiveCategories = (StatCategory)((u8)s_ActiveCategories & ~(u8)cat);
}

bool StatOverlay::IsEnabled(StatCategory cat) {
    return ((u8)s_ActiveCategories & (u8)cat) != 0;
}

void StatOverlay::ToggleByName(const std::string& name) {
    if (name == "fps")       Toggle(StatCategory::FPS);
    else if (name == "unit") Toggle(StatCategory::Unit);
    else if (name == "gpu")  Toggle(StatCategory::GPU);
    else if (name == "memory" || name == "mem") Toggle(StatCategory::Memory);
    else if (name == "rendering" || name == "render") Toggle(StatCategory::Rendering);
    else if (name == "physics" || name == "phys") Toggle(StatCategory::Physics);
    else if (name == "audio") Toggle(StatCategory::Audio);
    else if (name == "sceneinfo" || name == "scene") Toggle(StatCategory::SceneInfo);
    else if (name == "all") {
        u8 all = 0xFF;
        bool anyOn = (u8)s_ActiveCategories != 0;
        s_ActiveCategories = anyOn ? StatCategory::None : (StatCategory)all;
    }
}

// ── 数据录入 ────────────────────────────────────────────────

void StatOverlay::Update(f32 deltaTime, f32 gameTimeMs, f32 renderTimeMs, f32 gpuTimeMs) {
    s_FrameTimeMs = deltaTime * 1000.0f;
    s_FPS = (deltaTime > 0) ? (1.0f / deltaTime) : 0;
    s_FrameTimeHistory.Push(s_FrameTimeMs);
    s_FrameTimeAvg = s_FrameTimeHistory.Average();

    s_GameTimeMs = gameTimeMs;
    s_RenderTimeMs = renderTimeMs;
    s_GPUTimeMs = gpuTimeMs;
    s_GameTimeHistory.Push(gameTimeMs);
    s_RenderTimeHistory.Push(renderTimeMs);
    s_GPUTimeHistory.Push(gpuTimeMs);
}

void StatOverlay::RecordGPUPass(const std::string& name, f32 timeMs) {
    for (auto& pass : s_GPUPasses) {
        if (pass.Name == name) {
            pass.TimeMs = timeMs;
            pass.History.Push(timeMs);
            return;
        }
    }
    GPUPassInfo info;
    info.Name = name;
    info.TimeMs = timeMs;
    info.History.Push(timeMs);
    s_GPUPasses.push_back(std::move(info));
}

void StatOverlay::RecordMemory(const std::string& label, size_t bytes, size_t totalCapacity) {
    for (auto& entry : s_MemoryEntries) {
        if (entry.Label == label) {
            entry.Bytes = bytes;
            entry.Capacity = totalCapacity;
            return;
        }
    }
    s_MemoryEntries.push_back({label, bytes, totalCapacity});
}

void StatOverlay::RecordRendering(u32 drawCalls, u32 triangles, u32 batches,
                                   u32 stateChanges, u32 culledObjects) {
    s_DrawCalls = drawCalls;
    s_Triangles = triangles;
    s_Batches = batches;
    s_StateChanges = stateChanges;
    s_CulledObjects = culledObjects;
}

void StatOverlay::RecordPhysics(u32 collisionPairs, u32 bvhNodes, f32 broadPhaseMs) {
    s_CollisionPairs = collisionPairs;
    s_BVHNodes = bvhNodes;
    s_BroadPhaseMs = broadPhaseMs;
}

void StatOverlay::RecordSceneInfo(u32 entities, u32 activeLights, u32 particleEmitters) {
    s_Entities = entities;
    s_ActiveLights = activeLights;
    s_ParticleEmitters = particleEmitters;
}

// ── 渲染 ────────────────────────────────────────────────────

void StatOverlay::Render() {
    if ((u8)s_ActiveCategories == 0) return;

    // 半透明覆盖窗口 (右上角)
    ImGuiIO& io = ImGui::GetIO();
    float panelW = 320.0f;
    float x = io.DisplaySize.x - panelW - 10.0f;
    float y = 10.0f;

    ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, 0), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                              ImGuiWindowFlags_NoResize |
                              ImGuiWindowFlags_NoMove |
                              ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoSavedSettings |
                              ImGuiWindowFlags_NoFocusOnAppearing |
                              ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (!ImGui::Begin("##StatOverlay", nullptr, flags)) { ImGui::End(); return; }

    if (IsEnabled(StatCategory::FPS))       RenderFPS();
    if (IsEnabled(StatCategory::Unit))      RenderUnit();
    if (IsEnabled(StatCategory::GPU))       RenderGPU();
    if (IsEnabled(StatCategory::Memory))    RenderMemory();
    if (IsEnabled(StatCategory::Rendering)) RenderRendering();
    if (IsEnabled(StatCategory::Physics))   RenderPhysics();
    if (IsEnabled(StatCategory::SceneInfo)) RenderSceneInfo();

    ImGui::End();
}

// ── stat fps ────────────────────────────────────────────────

void StatOverlay::RenderFPS() {
    // 颜色: 绿 >60, 黄 >30, 红 <30
    ImVec4 fpsColor = (s_FPS >= 60) ? ImVec4(0.2f, 1.0f, 0.3f, 1) :
                      (s_FPS >= 30) ? ImVec4(1.0f, 0.9f, 0.2f, 1) :
                                      ImVec4(1.0f, 0.3f, 0.2f, 1);

    ImGui::PushStyleColor(ImGuiCol_Text, fpsColor);
    ImGui::Text("%.0f FPS", s_FPS);
    ImGui::PopStyleColor();

    ImGui::SameLine(80);
    ImGui::TextColored(ImVec4(0.7f,0.7f,0.7f,1), "%.2f ms", s_FrameTimeMs);
    ImGui::SameLine(170);
    ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "avg %.2f ms", s_FrameTimeAvg);

    // 迷你折线图
    float data[HISTORY_SIZE];
    u32 count = s_FrameTimeHistory.Count();
    s_FrameTimeHistory.CopyToArray(data, count);
    DrawMiniGraph("##fps_graph", data, count, 0, 50.0f,
                  ImVec2(ImGui::GetContentRegionAvail().x, 35),
                  IM_COL32(100, 200, 100, 255), true);

    ImGui::Separator();
}

// ── stat unit ───────────────────────────────────────────────

void StatOverlay::RenderUnit() {
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1), "STAT UNIT");

    f32 total = s_GameTimeMs + s_RenderTimeMs + s_GPUTimeMs;
    f32 maxBar = std::max(total, 33.3f);  // 至少 30fps 标度

    // 分时条
    auto DrawBar = [&](const char* label, f32 ms, ImVec4 color) {
        ImGui::TextColored(color, "%-8s %6.2f ms", label, ms);
        float fraction = ms / maxBar;
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
        ImGui::ProgressBar(fraction, ImVec2(-1, 8), "");
        ImGui::PopStyleColor();
    };

    DrawBar("Game",   s_GameTimeMs,   ImVec4(0.3f, 0.7f, 1.0f, 1));
    DrawBar("Render", s_RenderTimeMs, ImVec4(0.3f, 1.0f, 0.5f, 1));
    DrawBar("GPU",    s_GPUTimeMs,    ImVec4(1.0f, 0.6f, 0.2f, 1));
    DrawBar("Total",  total,          ImVec4(0.8f, 0.8f, 0.8f, 1));

    // 33ms / 16ms 参考线
    ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "30fps=33.3ms | 60fps=16.7ms");
    ImGui::Separator();
}

// ── stat gpu ────────────────────────────────────────────────

void StatOverlay::RenderGPU() {
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1), "STAT GPU");

    if (s_GPUPasses.empty()) {
        ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "  无 GPU 计时数据");
    } else {
        f32 totalGPU = 0;
        for (auto& pass : s_GPUPasses) totalGPU += pass.TimeMs;

        for (auto& pass : s_GPUPasses) {
            f32 pct = (totalGPU > 0) ? (pass.TimeMs / totalGPU * 100.0f) : 0;

            // 颜色按耗时占比
            ImVec4 color = (pct > 40) ? ImVec4(1, 0.3f, 0.3f, 1) :
                           (pct > 20) ? ImVec4(1, 0.8f, 0.2f, 1) :
                                        ImVec4(0.7f, 0.7f, 0.7f, 1);

            ImGui::TextColored(color, "  %-16s %5.2f ms (%4.1f%%)",
                              pass.Name.c_str(), pass.TimeMs, pct);
        }

        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1), "  Total: %.2f ms", totalGPU);
    }
    ImGui::Separator();
}

// ── stat memory ─────────────────────────────────────────────

void StatOverlay::RenderMemory() {
    ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1), "STAT MEMORY");

    size_t totalUsed = 0, totalCapacity = 0;

    for (auto& entry : s_MemoryEntries) {
        float sizeMB = (float)entry.Bytes / (1024.0f * 1024.0f);
        float capMB = (float)entry.Capacity / (1024.0f * 1024.0f);
        float usage = (entry.Capacity > 0) ? (float)entry.Bytes / (float)entry.Capacity : 0;

        // 颜色按使用率
        ImVec4 color = (usage > 0.8f) ? ImVec4(1, 0.3f, 0.3f, 1) :
                       (usage > 0.5f) ? ImVec4(1, 0.8f, 0.2f, 1) :
                                        ImVec4(0.5f, 0.9f, 0.5f, 1);

        ImGui::TextColored(color, "  %-12s %6.1f / %6.1f MB (%3.0f%%)",
                          entry.Label.c_str(), sizeMB, capMB, usage * 100.0f);

        totalUsed += entry.Bytes;
        totalCapacity += entry.Capacity;
    }

    float totalMB = (float)totalUsed / (1024.0f * 1024.0f);
    ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1), "  Total: %.1f MB", totalMB);
    ImGui::Separator();
}

// ── stat rendering ──────────────────────────────────────────

void StatOverlay::RenderRendering() {
    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1), "STAT RENDERING");
    ImGui::Text("  Draw Calls:    %u", s_DrawCalls);
    ImGui::Text("  Triangles:     %u", s_Triangles);
    ImGui::Text("  Batches:       %u", s_Batches);
    ImGui::Text("  State Changes: %u", s_StateChanges);
    ImGui::Text("  Culled:        %u", s_CulledObjects);
    ImGui::Separator();
}

// ── stat physics ────────────────────────────────────────────

void StatOverlay::RenderPhysics() {
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1), "STAT PHYSICS");
    ImGui::Text("  Collision Pairs: %u", s_CollisionPairs);
    ImGui::Text("  BVH Nodes:       %u", s_BVHNodes);
    ImGui::Text("  Broad Phase:     %.2f ms", s_BroadPhaseMs);
    ImGui::Separator();
}

// ── stat sceneinfo ──────────────────────────────────────────

void StatOverlay::RenderSceneInfo() {
    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1), "STAT SCENE");
    ImGui::Text("  Entities:       %u", s_Entities);
    ImGui::Text("  Active Lights:  %u", s_ActiveLights);
    ImGui::Text("  Particle Emit:  %u", s_ParticleEmitters);
    ImGui::Separator();
}

// ── 迷你折线图 ──────────────────────────────────────────────

void StatOverlay::DrawMiniGraph(const char* label, const float* data, u32 count,
                                 f32 minVal, f32 maxVal, ImVec2 size,
                                 ImU32 color, bool showSpikes) {
    if (count < 2) return;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 背景
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y),
                      IM_COL32(20, 20, 20, 180));

    f32 range = maxVal - minVal;
    if (range < 1e-6f) range = 1.0f;

    // 16.7ms / 33.3ms 参考线
    auto DrawRefLine = [&](f32 ms, ImU32 lineColor) {
        f32 y = pos.y + size.y - ((ms - minVal) / range) * size.y;
        if (y > pos.y && y < pos.y + size.y) {
            dl->AddLine(ImVec2(pos.x, y), ImVec2(pos.x + size.x, y), lineColor, 1.0f);
        }
    };
    DrawRefLine(16.7f, IM_COL32(0, 200, 0, 80));   // 60fps
    DrawRefLine(33.3f, IM_COL32(200, 200, 0, 80));  // 30fps

    // 求平均 (用于尖峰检测)
    f32 avg = 0;
    for (u32 i = 0; i < count; i++) avg += data[i];
    avg /= (f32)count;

    // 绘制折线
    for (u32 i = 1; i < count; i++) {
        f32 t0 = (f32)(i-1) / (f32)(count-1);
        f32 t1 = (f32)i / (f32)(count-1);

        f32 v0 = (data[i-1] - minVal) / range;
        f32 v1 = (data[i] - minVal) / range;
        v0 = std::clamp(v0, 0.0f, 1.0f);
        v1 = std::clamp(v1, 0.0f, 1.0f);

        ImVec2 p0(pos.x + t0 * size.x, pos.y + size.y - v0 * size.y);
        ImVec2 p1(pos.x + t1 * size.x, pos.y + size.y - v1 * size.y);

        // 尖峰高亮
        ImU32 lineColor = color;
        if (showSpikes && data[i] > avg * 2.0f) {
            lineColor = IM_COL32(255, 50, 50, 255);
        }

        dl->AddLine(p0, p1, lineColor, 1.5f);
    }

    ImGui::Dummy(size);
}

} // namespace Engine
