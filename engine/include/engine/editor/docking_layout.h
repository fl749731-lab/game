#pragma once

#include "engine/core/types.h"

#include <imgui.h>
#include <string>

namespace Engine {

// ── ImGui Docking 布局管理 ──────────────────────────────────
// 功能:
//   1. 初始化 Docking space
//   2. 保存/加载布局到 ini 文件
//   3. 预置布局 (Default / Game / Debug)
//   4. 菜单栏集成

class DockingLayout {
public:
    static void Init();
    static void Shutdown();

    /// 在帧开始时调用 — 创建全屏 DockSpace
    static void BeginFrame();

    /// 在帧结束前调用
    static void EndFrame();

    /// 应用预置布局
    enum Preset : u8 {
        Default,  // 标准编辑器布局
        Game,     // 游戏视图为主
        Debug,    // 调试面板为主
    };
    static void ApplyPreset(Preset preset);

    /// 保存/加载布局
    static void SaveLayout(const std::string& filename = "layout.ini");
    static void LoadLayout(const std::string& filename = "layout.ini");

    /// 渲染主菜单栏
    static void RenderMainMenuBar();

    /// 切换全屏游戏模式
    static void SetGameMode(bool enabled);
    static bool IsGameMode() { return s_GameMode; }

private:
    inline static ImGuiID s_DockSpaceID = 0;
    inline static bool s_GameMode = false;
    inline static bool s_FirstTime = true;
};

} // namespace Engine
