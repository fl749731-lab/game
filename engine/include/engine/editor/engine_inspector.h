#pragma once

#include "engine/core/types.h"

namespace Engine {

// ── ImGui 引擎检查器 ───────────────────────────────────────
// 提供 CSM / 体积光 / 物理 / Bloom / SSAO / SSR 的实时参数调节
// 使用 Dear ImGui 渲染

class EngineInspector {
public:
    static void Init();
    static void Shutdown();

    /// 每帧绘制检查器面板 (在 ImGui::NewFrame 与 ImGui::Render 之间调用)
    static void Draw();

    static void SetVisible(bool visible);
    static bool IsVisible();

private:
    static void DrawRendererPanel();
    static void DrawCSMPanel();
    static void DrawVolumetricPanel();
    static void DrawPhysicsPanel();
    static void DrawBloomPanel();
    static void DrawSSAOPanel();
    static void DrawSSRPanel();
    static void DrawProfilerPanel();

    static bool s_Visible;
};

} // namespace Engine
