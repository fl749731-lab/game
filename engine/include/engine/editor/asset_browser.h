#pragma once

#include "engine/core/types.h"

#include <imgui.h>
#include <string>
#include <vector>
#include <filesystem>

namespace Engine {

// ── 资源浏览器面板 ──────────────────────────────────────────
// 功能:
//   1. 文件系统树形浏览
//   2. 资源缩略图网格 (纹理/模型/材质)
//   3. 双击打开/加载资源
//   4. 搜索过滤
//   5. 文件类型图标颜色区分
//   6. 路径导航面包屑

class AssetBrowser {
public:
    static void Init(const std::string& rootPath = "assets");
    static void Shutdown();

    /// 渲染资源浏览器面板
    static void Render();

    /// 获取当前目录
    static const std::string& GetCurrentPath() { return s_CurrentPath; }

    /// 设置根目录
    static void SetRootPath(const std::string& path) { s_RootPath = path; NavigateTo(path); }

    /// 导航到目录
    static void NavigateTo(const std::string& path);

    /// 上层目录
    static void NavigateUp();

private:
    struct FileEntry {
        std::string Name;
        std::string FullPath;
        std::string Extension;
        bool IsDirectory = false;
        u64  FileSize = 0;

        // 图标颜色
        ImU32 IconColor = IM_COL32(180, 180, 180, 255);
    };

    static void RefreshDirectory();
    static void RenderBreadcrumb();
    static void RenderFileGrid();
    static void RenderFileList();
    static ImU32 GetExtensionColor(const std::string& ext);
    static const char* GetExtensionIcon(const std::string& ext);

    inline static std::string s_RootPath = "assets";
    inline static std::string s_CurrentPath = "assets";
    inline static std::vector<FileEntry> s_Entries;
    inline static char s_SearchBuffer[256] = {};
    inline static bool s_GridMode = true;
    inline static float s_ThumbnailSize = 80.0f;
};

} // namespace Engine
