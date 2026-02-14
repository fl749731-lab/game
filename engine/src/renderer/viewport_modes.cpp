#include "engine/renderer/viewport_modes.h"
#include "engine/renderer/scene_renderer.h"
#include "engine/core/log.h"

#include <imgui.h>

namespace Engine {

static const char* s_ModeNames[] = {
    "Lit", "Unlit", "Wireframe", "Normals", "UV",
    "Overdraw", "Depth", "Light Complexity"
};

static const ImVec4 s_ModeColors[] = {
    {0.8f, 0.8f, 0.8f, 1},  // Lit
    {0.6f, 0.6f, 0.6f, 1},  // Unlit
    {0.3f, 1.0f, 0.3f, 1},  // Wireframe
    {0.5f, 0.5f, 1.0f, 1},  // Normals
    {1.0f, 1.0f, 0.3f, 1},  // UV
    {1.0f, 0.3f, 0.3f, 1},  // Overdraw
    {0.7f, 0.7f, 0.7f, 1},  // Depth
    {1.0f, 0.6f, 0.2f, 1},  // LightComplexity
};

void ViewportModes::Init() {
    s_CurrentMode = ViewportMode::Lit;
    LOG_INFO("[ViewportModes] 初始化 | 8 种可视化模式");
}

void ViewportModes::Shutdown() {
    LOG_INFO("[ViewportModes] 关闭");
}

void ViewportModes::SetMode(ViewportMode mode) {
    if (mode != s_CurrentMode) {
        LOG_INFO("[ViewportModes] 切换到: %s", GetModeName(mode));
        s_CurrentMode = mode;

        // 同步 SceneRenderer 的 GBuffer 调试模式
        i32 gbufMode = GetGBufferOverride();
        if (gbufMode >= 0) {
            SceneRenderer::SetGBufferDebugMode(gbufMode);
        } else {
            SceneRenderer::SetGBufferDebugMode(0);
        }

        // 线框
        SceneRenderer::SetWireframe(mode == ViewportMode::Wireframe);
    }
}

ViewportMode ViewportModes::GetMode() { return s_CurrentMode; }

const char* ViewportModes::GetModeName(ViewportMode mode) {
    u8 idx = (u8)mode;
    return (idx < (u8)ViewportMode::Count) ? s_ModeNames[idx] : "Unknown";
}

bool ViewportModes::HandleKeyInput(int key, int action, bool altDown) {
    if (!altDown || action != 1) return false;

    // Alt+1~8 → ViewportMode 0~7
    // GLFW_KEY_1 = 49 ... GLFW_KEY_8 = 56
    if (key >= 49 && key <= 56) {
        u8 modeIdx = (u8)(key - 49);
        if (modeIdx < (u8)ViewportMode::Count) {
            SetMode((ViewportMode)modeIdx);
            return true;
        }
    }
    return false;
}

i32 ViewportModes::GetGBufferOverride() {
    switch (s_CurrentMode) {
        case ViewportMode::Lit:       return 0;  // 正常
        case ViewportMode::Unlit:     return 3;  // Albedo only
        case ViewportMode::Normals:   return 2;  // Normals
        case ViewportMode::Depth:     return 1;  // Position (depth implicit)
        default: return -1;
    }
}

bool ViewportModes::NeedsWireframeOverlay() {
    return s_CurrentMode == ViewportMode::Wireframe;
}

bool ViewportModes::NeedsSpecialPass() {
    return s_CurrentMode == ViewportMode::Overdraw ||
           s_CurrentMode == ViewportMode::LightComplexity;
}

void ViewportModes::BeginOverdrawPass() {
    // TODO: 绑定 Overdraw shader + 加法混合
}

void ViewportModes::EndOverdrawPass() {
    // TODO: 解绑 + 读回纹理
}

u32 ViewportModes::GetOverdrawTextureID() { return s_OverdrawTexture; }

// ── ImGui 工具栏 ────────────────────────────────────────────

void ViewportModes::RenderToolbar() {
    ImGui::Text("视口模式:");
    ImGui::SameLine();

    for (u8 i = 0; i < (u8)ViewportMode::Count; i++) {
        bool selected = ((u8)s_CurrentMode == i);
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.2f, 1));
            ImGui::PushStyleColor(ImGuiCol_Text, s_ModeColors[i]);
        }

        char label[32];
        snprintf(label, sizeof(label), "%d:%s", i+1, s_ModeNames[i]);
        if (ImGui::SmallButton(label)) {
            SetMode((ViewportMode)i);
        }
        ImGui::PopStyleColor(2);

        if (i < (u8)ViewportMode::Count - 1) ImGui::SameLine();
    }
}

} // namespace Engine
