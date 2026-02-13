#pragma once

#include "engine/core/types.h"
#include "engine/core/scene.h"

struct GLFWwindow;

namespace Engine {

// ── 场景编辑器 (ImGui) ──────────────────────────────────────

class Editor {
public:
    static void Init(GLFWwindow* window);
    static void Shutdown();

    /// 每帧开始（NewFrame）
    static void BeginFrame();

    /// 渲染所有编辑器面板
    static void Render(Scene& scene, Entity& selectedEntity);

    /// 每帧结束（ImGui::Render + 绘制数据）
    static void EndFrame();

    static bool IsEnabled() { return s_Enabled; }
    static void SetEnabled(bool v) { s_Enabled = v; }
    static void Toggle() { s_Enabled = !s_Enabled; }

private:
    static void DrawSceneHierarchy(Scene& scene, Entity& selectedEntity);
    static void DrawInspector(ECSWorld& world, Entity entity);
    static void DrawLightEditor(Scene& scene);
    static void DrawPerformance(Scene& scene);

    static bool s_Enabled;
};

} // namespace Engine
