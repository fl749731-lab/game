#include "engine/editor/docking_layout.h"
#include "engine/core/log.h"

#include <imgui.h>

namespace Engine {

void DockingLayout::Init() {
    s_FirstTime = true;
    LOG_INFO("[DockingLayout] 初始化 (简化模式 — ImGui 无 Docking 分支)");
}

void DockingLayout::Shutdown() {
    LOG_INFO("[DockingLayout] 关闭");
}

void DockingLayout::BeginFrame() {
    if (s_GameMode) return;

    // 简化模式: 直接创建一个全屏窗口作为"DockSpace"容器
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##MainContainer", nullptr, flags);
    ImGui::PopStyleVar(3);

    s_DockSpaceID = ImGui::GetID("EngineDockSpace");

    RenderMainMenuBar();
}

void DockingLayout::EndFrame() {
    if (!s_GameMode) {
        ImGui::End(); // MainContainer
    }
}

void DockingLayout::ApplyPreset(Preset preset) {
    // 简化模式下不实际对接 DockBuilder
    // 只记录偏好，面板在各自 Render 中独立渲染
    LOG_INFO("[DockingLayout] 应用布局预设 %d (简化模式)", (int)preset);
}

void DockingLayout::SaveLayout(const std::string& filename) {
    ImGui::SaveIniSettingsToDisk(filename.c_str());
    LOG_INFO("[DockingLayout] 布局已保存: %s", filename.c_str());
}

void DockingLayout::LoadLayout(const std::string& filename) {
    ImGui::LoadIniSettingsFromDisk(filename.c_str());
    LOG_INFO("[DockingLayout] 布局已加载: %s", filename.c_str());
}

void DockingLayout::SetGameMode(bool enabled) {
    s_GameMode = enabled;
    LOG_INFO("[DockingLayout] 游戏模式: %s", enabled ? "ON" : "OFF");
}

void DockingLayout::RenderMainMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("文件")) {
            if (ImGui::MenuItem("保存布局")) SaveLayout();
            if (ImGui::MenuItem("加载布局")) LoadLayout();
            ImGui::Separator();
            if (ImGui::MenuItem("退出", "Alt+F4")) {
                // 请求退出
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("视图")) {
            if (ImGui::MenuItem("默认布局")) ApplyPreset(Default);
            if (ImGui::MenuItem("游戏布局")) ApplyPreset(Game);
            if (ImGui::MenuItem("调试布局")) ApplyPreset(Debug);
            ImGui::Separator();
            if (ImGui::MenuItem("全屏游戏", "F11", s_GameMode)) {
                SetGameMode(!s_GameMode);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("工具")) {
            ImGui::MenuItem("层级面板");
            ImGui::MenuItem("属性面板");
            ImGui::MenuItem("控制台");
            ImGui::MenuItem("性能分析");
            ImGui::MenuItem("资产浏览器");
            ImGui::MenuItem("材质编辑器");
            ImGui::MenuItem("曲线编辑器");
            ImGui::MenuItem("节点图");
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

} // namespace Engine
