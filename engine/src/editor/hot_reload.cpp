#include "engine/editor/hot_reload.h"
#include "engine/core/log.h"

#include <imgui.h>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace Engine {

void HotReloadSystem::Init() {
    s_WatchedFiles.clear();
    s_FileTimestamps.clear();
    s_ReloadCount = 0;
    s_RecentReloads.clear();
    LOG_INFO("[HotReload] 初始化 | 检查间隔: %.1f秒", s_CheckInterval);
}

void HotReloadSystem::Shutdown() {
    UnwatchAll();
    LOG_INFO("[HotReload] 关闭 | 总重载: %u 次", s_ReloadCount);
}

void HotReloadSystem::Update(f32 dt) {
    s_TimeSinceCheck += dt;
    if (s_TimeSinceCheck >= s_CheckInterval) {
        s_TimeSinceCheck = 0;
        CheckFileChanges();
    }
}

void HotReloadSystem::WatchDirectory(const std::string& path) {
    if (!fs::exists(path) || !fs::is_directory(path)) {
        LOG_WARN("[HotReload] 目录不存在: %s", path.c_str());
        return;
    }

    for (auto& entry : fs::recursive_directory_iterator(path)) {
        if (entry.is_regular_file()) {
            std::string filePath = entry.path().string();
            s_FileTimestamps[filePath] = entry.last_write_time();
        }
    }

    WatchedFile wf;
    wf.Path = path;
    wf.IsDirectory = true;
    s_WatchedFiles.push_back(wf);

    LOG_INFO("[HotReload] 监视目录: %s", path.c_str());
}

void HotReloadSystem::WatchFile(const std::string& path) {
    if (!fs::exists(path)) {
        LOG_WARN("[HotReload] 文件不存在: %s", path.c_str());
        return;
    }

    WatchedFile wf;
    wf.Path = path;
    wf.LastModified = fs::last_write_time(path);
    wf.IsDirectory = false;
    s_WatchedFiles.push_back(wf);
    s_FileTimestamps[path] = wf.LastModified;

    LOG_INFO("[HotReload] 监视文件: %s", path.c_str());
}

void HotReloadSystem::UnwatchAll() {
    s_WatchedFiles.clear();
    s_FileTimestamps.clear();
    LOG_INFO("[HotReload] 清除所有监视");
}

void HotReloadSystem::OnShaderChanged(ReloadCallback cb) { s_ShaderCallback = cb; }
void HotReloadSystem::OnScriptChanged(ReloadCallback cb) { s_ScriptCallback = cb; }
void HotReloadSystem::OnConfigChanged(ReloadCallback cb) { s_ConfigCallback = cb; }
void HotReloadSystem::OnAnyFileChanged(ReloadCallback cb) { s_AnyCallback = cb; }

void HotReloadSystem::ForceReloadAll() {
    LOG_INFO("[HotReload] 强制全量重载");
    for (auto& [path, _] : s_FileTimestamps) {
        OnFileChanged(path);
    }
}

void HotReloadSystem::ForceReloadShaders() {
    LOG_INFO("[HotReload] 强制重载所有 Shader");
    for (auto& [path, _] : s_FileTimestamps) {
        if (IsShaderFile(GetExtension(path)))
            OnFileChanged(path);
    }
}

void HotReloadSystem::ForceReloadScripts() {
    LOG_INFO("[HotReload] 强制重载所有脚本");
    for (auto& [path, _] : s_FileTimestamps) {
        if (IsScriptFile(GetExtension(path)))
            OnFileChanged(path);
    }
}

void HotReloadSystem::CheckFileChanges() {
    for (auto& wf : s_WatchedFiles) {
        if (wf.IsDirectory) {
            // 遍历目录
            if (!fs::exists(wf.Path)) continue;
            for (auto& entry : fs::recursive_directory_iterator(wf.Path)) {
                if (!entry.is_regular_file()) continue;
                std::string filePath = entry.path().string();
                auto modTime = entry.last_write_time();

                auto it = s_FileTimestamps.find(filePath);
                if (it == s_FileTimestamps.end()) {
                    // 新文件
                    s_FileTimestamps[filePath] = modTime;
                    OnFileChanged(filePath);
                } else if (it->second != modTime) {
                    // 已修改
                    it->second = modTime;
                    OnFileChanged(filePath);
                }
            }
        } else {
            // 单个文件
            if (!fs::exists(wf.Path)) continue;
            auto modTime = fs::last_write_time(wf.Path);
            if (modTime != wf.LastModified) {
                wf.LastModified = modTime;
                s_FileTimestamps[wf.Path] = modTime;
                OnFileChanged(wf.Path);
            }
        }
    }
}

void HotReloadSystem::OnFileChanged(const std::string& path) {
    std::string ext = GetExtension(path);
    s_ReloadCount++;
    s_RecentReloads.push_back(fs::path(path).filename().string());
    if (s_RecentReloads.size() > 20) s_RecentReloads.erase(s_RecentReloads.begin());

    LOG_INFO("[HotReload] 文件变更: %s", path.c_str());

    if (IsShaderFile(ext) && s_ShaderCallback) {
        s_ShaderCallback(path);
    } else if (IsScriptFile(ext) && s_ScriptCallback) {
        s_ScriptCallback(path);
    } else if (IsConfigFile(ext) && s_ConfigCallback) {
        s_ConfigCallback(path);
    }

    if (s_AnyCallback) s_AnyCallback(path);
}

std::string HotReloadSystem::GetExtension(const std::string& path) {
    fs::path fp(path);
    std::string ext = fp.extension().string();
    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

bool HotReloadSystem::IsShaderFile(const std::string& ext) {
    return ext == "glsl" || ext == "vert" || ext == "frag"
        || ext == "geom" || ext == "comp" || ext == "vs" || ext == "fs";
}

bool HotReloadSystem::IsScriptFile(const std::string& ext) {
    return ext == "py" || ext == "lua";
}

bool HotReloadSystem::IsConfigFile(const std::string& ext) {
    return ext == "json" || ext == "xml" || ext == "yaml" || ext == "ini" || ext == "cfg";
}

void HotReloadSystem::RenderStatusPanel() {
    if (!ImGui::Begin("热重载##HotReload")) { ImGui::End(); return; }

    ImGui::Text("状态: %s", IsWatching() ? "监视中" : "未启动");
    ImGui::Text("监视项: %u", (u32)s_WatchedFiles.size());
    ImGui::Text("总重载: %u", s_ReloadCount);

    ImGui::Separator();

    if (ImGui::Button("强制重载 Shader")) ForceReloadShaders();
    ImGui::SameLine();
    if (ImGui::Button("强制重载脚本")) ForceReloadScripts();
    if (ImGui::Button("强制全量重载")) ForceReloadAll();

    ImGui::Separator();
    ImGui::Text("最近变更:");
    for (auto it = s_RecentReloads.rbegin(); it != s_RecentReloads.rend(); ++it) {
        ImGui::BulletText("%s", it->c_str());
    }

    ImGui::End();
}

} // namespace Engine
