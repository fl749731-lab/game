#include "engine/editor/scene_view.h"
#include "engine/core/log.h"

#include <imgui.h>

namespace Engine {

void SceneView::Init() {
    LOG_INFO("[SceneView] 初始化");
}

void SceneView::Shutdown() {
    LOG_INFO("[SceneView] 关闭");
}

void SceneView::Render(u32 hdrColorTextureID, f32 viewportAspect) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    if (!ImGui::Begin("场景视口##SceneView", nullptr, flags)) {
        ImGui::PopStyleVar();
        ImGui::End();
        return;
    }

    // 工具栏 (在无 padding 前先恢复)
    ImGui::PopStyleVar();
    RenderToolbar();
    ImGui::Separator();

    // 视口区域
    ImVec2 viewportSize = ImGui::GetContentRegionAvail();
    if (viewportSize.x < 1) viewportSize.x = 1;
    if (viewportSize.y < 1) viewportSize.y = 1;

    s_ViewportWidth  = (u32)viewportSize.x;
    s_ViewportHeight = (u32)viewportSize.y;

    // Blit HDR FBO 到 ImGui
    if (hdrColorTextureID > 0) {
        ImGui::Image((ImTextureID)(intptr_t)hdrColorTextureID,
                     viewportSize,
                     ImVec2(0, 1), ImVec2(1, 0));  // OpenGL UV 翻转
    } else {
        // 无纹理时显示占位背景
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1(p0.x + viewportSize.x, p0.y + viewportSize.y);
        dl->AddRectFilled(p0, p1, IM_COL32(25, 25, 30, 255));
        dl->AddText(ImVec2(p0.x + viewportSize.x * 0.5f - 40,
                           p0.y + viewportSize.y * 0.5f - 8),
                    IM_COL32(100, 100, 120, 255), "No Scene");
    }

    s_Hovered = ImGui::IsItemHovered();
    s_Focused = ImGui::IsWindowFocused();

    // 叠加信息
    if (s_ShowStats)
        RenderViewportInfo();

    ImGui::End();
}

void SceneView::RenderToolbar() {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.2f, 1));

    // Grid toggle
    if (ImGui::SmallButton(s_ShowGrid ? "Grid ON" : "Grid OFF")) ToggleGrid();
    ImGui::SameLine();

    // Stats toggle
    if (ImGui::SmallButton(s_ShowStats ? "Stats ON" : "Stats OFF")) s_ShowStats = !s_ShowStats;
    ImGui::SameLine();

    ImGui::Text("|");
    ImGui::SameLine();
    ImGui::Text("%ux%u", s_ViewportWidth, s_ViewportHeight);

    ImGui::PopStyleColor();
}

void SceneView::RenderViewportInfo() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    float windowH = ImGui::GetWindowHeight();

    // 左下角半透明信息
    ImVec2 infoPos(wp.x + 8, wp.y + windowH - 24);
    dl->AddRectFilled(ImVec2(infoPos.x - 4, infoPos.y - 2),
                      ImVec2(infoPos.x + 200, infoPos.y + 16),
                      IM_COL32(0, 0, 0, 150), 4.0f);

    char buf[128];
    snprintf(buf, sizeof(buf), "视口: %ux%u | %s",
             s_ViewportWidth, s_ViewportHeight,
             s_ShowGrid ? "Grid" : "NoGrid");
    dl->AddText(infoPos, IM_COL32(180, 180, 200, 200), buf);
}

} // namespace Engine
