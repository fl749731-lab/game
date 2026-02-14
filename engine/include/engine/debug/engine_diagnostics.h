#pragma once

#include "engine/core/types.h"
#include "engine/debug/gpu_profiler.h"

#include <imgui.h>
#include <string>
#include <vector>
#include <functional>

namespace Engine {

// ── 引擎 Insight 面板 (UE 级诊断) ──────────────────────────
// 重写 EngineDiagnostics: 从简单面板 → UE Insight 级工具
//
// 子面板:
//   1. 渲染目标浏览器 (GBuf MRT / ShadowMap / HDR / Bloom 预览)
//   2. CPU/GPU 火焰图 (层级式分析)
//   3. 纹理浏览器 (所有已加载纹理 + VRAM)
//   4. DrawCall 分析 (按 Shader/Material 分组)
//   5. 帧历史 + 回溯滑块
//   6. 着色器热点 (每材质 GPU 耗时)

class EngineDiagnostics {
public:
    static void Init();
    static void Shutdown();

    /// ImGui 渲染所有激活的子面板
    static void Render();

    /// 子面板切换
    static void ToggleRenderTargets();
    static void ToggleFlameGraph();
    static void ToggleTextureBrowser();
    static void ToggleDrawCallAnalysis();
    static void ToggleFrameHistory();

    // ── 渲染目标浏览器 ──────────────────────────────

    struct RenderTargetInfo {
        std::string Name;
        u32 TextureID = 0;
        u32 Width = 0, Height = 0;
        std::string Format;   // "RGBA16F", "DEPTH24" etc.
    };

    static void RegisterRenderTarget(const std::string& name, u32 texID,
                                      u32 w, u32 h, const std::string& format);
    static void ClearRenderTargets();

    // ── 火焰图数据 ──────────────────────────────────

    struct FlameEntry {
        std::string Name;
        f32 StartMs;
        f32 DurationMs;
        u32 Depth;
        ImVec4 Color;
    };

    static void RecordFlameEntry(const std::string& name, f32 startMs,
                                  f32 durationMs, u32 depth);
    static void ClearFlameEntries();

    // ── 纹理浏览器 ──────────────────────────────────

    struct TextureInfo {
        std::string Name;
        u32 TextureID = 0;
        u32 Width = 0, Height = 0;
        size_t VRAMBytes = 0;
        std::string Format;
    };

    static void RegisterTexture(const std::string& name, u32 texID,
                                 u32 w, u32 h, size_t vram,
                                 const std::string& format);
    static void ClearTextures();

    // ── DrawCall 分析 ───────────────────────────────

    struct DrawCallGroup {
        std::string ShaderName;
        std::string MaterialName;
        u32 DrawCalls = 0;
        u32 Triangles = 0;
        f32 GPUTimeMs = 0;
    };

    static void RecordDrawCallGroup(const std::string& shader,
                                     const std::string& material,
                                     u32 draws, u32 tris, f32 gpuMs);
    static void ClearDrawCallGroups();

    // ── 帧历史滑块 ─────────────────────────────────

    struct FrameSnapshot {
        f32 TotalMs = 0;
        f32 CPUMs = 0;
        f32 GPUMs = 0;
        u32 DrawCalls = 0;
        u32 Triangles = 0;
        std::vector<FlameEntry> FlameData;
    };

    static void PushFrameSnapshot(const FrameSnapshot& snapshot);
    static void SetFrameHistoryPaused(bool paused);
    static bool IsFrameHistoryPaused();

private:
    static void RenderRenderTargets();
    static void RenderFlameGraph();
    static void RenderTextureBrowser();
    static void RenderDrawCallAnalysis();
    static void RenderFrameHistory();

    // 火焰图绘制辅助
    static ImU32 GetFlameColor(u32 depth, const std::string& name);

    inline static bool s_ShowRenderTargets = false;
    inline static bool s_ShowFlameGraph = false;
    inline static bool s_ShowTextureBrowser = false;
    inline static bool s_ShowDrawCallAnalysis = false;
    inline static bool s_ShowFrameHistory = false;

    inline static std::vector<RenderTargetInfo> s_RenderTargets;
    inline static std::vector<FlameEntry> s_FlameEntries;
    inline static std::vector<TextureInfo> s_Textures;
    inline static std::vector<DrawCallGroup> s_DrawCallGroups;

    // 帧历史
    static constexpr u32 MAX_FRAME_HISTORY = 300;
    inline static std::vector<FrameSnapshot> s_FrameHistory;
    inline static i32 s_SelectedFrame = -1;  // -1 = 最新帧
    inline static bool s_HistoryPaused = false;
};

} // namespace Engine
