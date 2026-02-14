#pragma once

#include "engine/core/types.h"

#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include <chrono>
#include <unordered_map>

namespace Engine {

// ── 热重载系统 ──────────────────────────────────────────────
// 功能:
//   1. 文件系统监视 (轮询方式)
//   2. Shader 热重载
//   3. Script (Python) 热重载
//   4. 配置文件热重载
//   5. 回调通知

class HotReloadSystem {
public:
    static void Init();
    static void Shutdown();

    /// 每帧调用 — 检查文件变化
    static void Update(f32 dt);

    // 监视路径
    static void WatchDirectory(const std::string& path);
    static void WatchFile(const std::string& path);
    static void UnwatchAll();

    // 回调注册
    using ReloadCallback = std::function<void(const std::string& path)>;
    static void OnShaderChanged(ReloadCallback cb);
    static void OnScriptChanged(ReloadCallback cb);
    static void OnConfigChanged(ReloadCallback cb);
    static void OnAnyFileChanged(ReloadCallback cb);

    // 强制全量重载
    static void ForceReloadAll();
    static void ForceReloadShaders();
    static void ForceReloadScripts();

    // 状态
    static u32  GetReloadCount() { return s_ReloadCount; }
    static bool IsWatching() { return !s_WatchedFiles.empty(); }

    /// 渲染热重载状态面板
    static void RenderStatusPanel();

private:
    struct WatchedFile {
        std::string Path;
        std::filesystem::file_time_type LastModified;
        bool IsDirectory = false;
    };

    static void CheckFileChanges();
    static void OnFileChanged(const std::string& path);

    static std::string GetExtension(const std::string& path);
    static bool IsShaderFile(const std::string& ext);
    static bool IsScriptFile(const std::string& ext);
    static bool IsConfigFile(const std::string& ext);

    inline static std::vector<WatchedFile> s_WatchedFiles;
    inline static std::unordered_map<std::string, std::filesystem::file_time_type> s_FileTimestamps;
    inline static ReloadCallback s_ShaderCallback;
    inline static ReloadCallback s_ScriptCallback;
    inline static ReloadCallback s_ConfigCallback;
    inline static ReloadCallback s_AnyCallback;

    inline static f32 s_CheckInterval = 1.0f;  // 秒
    inline static f32 s_TimeSinceCheck = 0.0f;
    inline static u32 s_ReloadCount = 0;
    inline static std::vector<std::string> s_RecentReloads;
};

} // namespace Engine
