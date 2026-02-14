// ── 引擎检查器 ──────────────────────────────────────────────
// 需要各渲染/物理系统暴露 GetConfig / IsEnabled / Get/Set 接口
// 在这些接口就绪前，此文件通过宏暂时跳过编译
//
// 启用: 在 CMake 中添加 -DENGINE_ENABLE_INSPECTOR=ON
// 或在 target_compile_definitions 中添加 ENGINE_ENABLE_INSPECTOR

#ifdef ENGINE_ENABLE_INSPECTOR

#include "engine/editor/engine_inspector.h"
#include "engine/renderer/scene_renderer.h"
#include "engine/core/log.h"

#include <imgui.h>

namespace Engine {

bool EngineInspector::s_Visible = false;

void EngineInspector::Init() {
    LOG_INFO("[EngineInspector] 初始化");
}

void EngineInspector::Shutdown() {
    LOG_DEBUG("[EngineInspector] 已清理");
}

void EngineInspector::SetVisible(bool visible) { s_Visible = visible; }
bool EngineInspector::IsVisible() { return s_Visible; }

void EngineInspector::Draw() {
    if (!s_Visible) return;

    ImGui::SetNextWindowSize(ImVec2(380, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("引擎检查器", &s_Visible)) {
        ImGui::End();
        return;
    }

    // ── 渲染器基础面板 ──────────────────────────────────────
    if (ImGui::CollapsingHeader("渲染器", ImGuiTreeNodeFlags_DefaultOpen))
        DrawRendererPanel();

    ImGui::End();
}

void EngineInspector::DrawRendererPanel() {
    f32 exposure = SceneRenderer::GetExposure();
    if (ImGui::SliderFloat("曝光", &exposure, 0.1f, 5.0f, "%.2f"))
        SceneRenderer::SetExposure(exposure);

    int debugMode = SceneRenderer::GetGBufferDebugMode();
    const char* debugModes[] = {"关闭", "Position", "Normal", "Albedo", "Specular", "Emissive"};
    if (ImGui::Combo("G-Buffer 调试", &debugMode, debugModes, 6))
        SceneRenderer::SetGBufferDebugMode(debugMode);

    auto& stats = SceneRenderer::GetFrameStats();
    ImGui::Text("DrawCalls: %u", stats.DrawCalls);
    ImGui::Text("三角形: %u", stats.TriangleCount);
}

// 桩函数 — 待各模块暴露 GetConfig 后实现
void EngineInspector::DrawCSMPanel() {}
void EngineInspector::DrawVolumetricPanel() {}
void EngineInspector::DrawPhysicsPanel() {}
void EngineInspector::DrawBloomPanel() {}
void EngineInspector::DrawSSAOPanel() {}
void EngineInspector::DrawSSRPanel() {}
void EngineInspector::DrawProfilerPanel() {}

} // namespace Engine

#endif // ENGINE_ENABLE_INSPECTOR
