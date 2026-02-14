#include "engine/editor/asset_browser.h"
#include "engine/core/log.h"

#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace Engine {

void AssetBrowser::Init(const std::string& rootPath) {
    s_RootPath = rootPath;
    s_CurrentPath = rootPath;
    RefreshDirectory();
    LOG_INFO("[AssetBrowser] 初始化 | 根目录: %s", rootPath.c_str());
}

void AssetBrowser::Shutdown() {
    s_Entries.clear();
    LOG_INFO("[AssetBrowser] 关闭");
}

void AssetBrowser::NavigateTo(const std::string& path) {
    s_CurrentPath = path;
    RefreshDirectory();
}

void AssetBrowser::NavigateUp() {
    std::filesystem::path p(s_CurrentPath);
    if (p.has_parent_path() && s_CurrentPath != s_RootPath) {
        s_CurrentPath = p.parent_path().string();
        RefreshDirectory();
    }
}

ImU32 AssetBrowser::GetExtensionColor(const std::string& ext) {
    if (ext == ".png" || ext == ".jpg" || ext == ".bmp" || ext == ".tga")
        return IM_COL32(100, 200, 255, 255);  // 纹理 — 蓝
    if (ext == ".glsl" || ext == ".vert" || ext == ".frag")
        return IM_COL32(255, 200, 80, 255);   // 着色器 — 黄
    if (ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx")
        return IM_COL32(80, 255, 80, 255);    // 模型 — 绿
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3")
        return IM_COL32(255, 120, 200, 255);  // 音频 — 粉
    if (ext == ".json" || ext == ".xml" || ext == ".yaml")
        return IM_COL32(200, 180, 255, 255);  // 配置 — 淡紫
    if (ext == ".cpp" || ext == ".h" || ext == ".c")
        return IM_COL32(255, 150, 50, 255);   // 代码 — 橙
    return IM_COL32(180, 180, 180, 255);       // 默认 — 灰
}

const char* AssetBrowser::GetExtensionIcon(const std::string& ext) {
    if (ext == ".png" || ext == ".jpg" || ext == ".bmp" || ext == ".tga") return "[IMG]";
    if (ext == ".glsl" || ext == ".vert" || ext == ".frag") return "[SHD]";
    if (ext == ".obj" || ext == ".gltf" || ext == ".glb") return "[MDL]";
    if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") return "[AUD]";
    if (ext == ".json" || ext == ".xml" || ext == ".yaml") return "[CFG]";
    return "[FIL]";
}

void AssetBrowser::RefreshDirectory() {
    s_Entries.clear();

    try {
        if (!std::filesystem::exists(s_CurrentPath)) return;

        for (auto& entry : std::filesystem::directory_iterator(s_CurrentPath)) {
            FileEntry fe;
            fe.FullPath = entry.path().string();
            fe.Name = entry.path().filename().string();
            fe.IsDirectory = entry.is_directory();

            if (!fe.IsDirectory) {
                fe.Extension = entry.path().extension().string();
                std::transform(fe.Extension.begin(), fe.Extension.end(), fe.Extension.begin(), ::tolower);
                fe.FileSize = entry.file_size();
                fe.IconColor = GetExtensionColor(fe.Extension);
            } else {
                fe.IconColor = IM_COL32(255, 220, 80, 255); // 文件夹 — 黄
            }

            s_Entries.push_back(fe);
        }

        // 排序: 文件夹优先, 然后按名称
        std::sort(s_Entries.begin(), s_Entries.end(), [](const FileEntry& a, const FileEntry& b) {
            if (a.IsDirectory != b.IsDirectory) return a.IsDirectory > b.IsDirectory;
            return a.Name < b.Name;
        });

    } catch (const std::exception& e) {
        LOG_ERROR("[AssetBrowser] 读取目录失败: %s", e.what());
    }
}

void AssetBrowser::Render() {
    if (!ImGui::Begin("资源浏览器##AssetBrowser")) { ImGui::End(); return; }

    // 上层目录 + 面包屑
    RenderBreadcrumb();
    ImGui::Separator();

    // 搜索栏
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("搜索##AssetSearch", s_SearchBuffer, sizeof(s_SearchBuffer));
    ImGui::SameLine();

    // 切换视图模式
    if (ImGui::SmallButton(s_GridMode ? "列表" : "网格")) s_GridMode = !s_GridMode;
    ImGui::SameLine();

    if (s_GridMode) {
        ImGui::SetNextItemWidth(100);
        ImGui::SliderFloat("大小", &s_ThumbnailSize, 40, 160);
    }

    ImGui::Separator();

    // 内容区
    if (s_GridMode)
        RenderFileGrid();
    else
        RenderFileList();

    ImGui::End();
}

void AssetBrowser::RenderBreadcrumb() {
    if (ImGui::SmallButton("<-")) NavigateUp();
    ImGui::SameLine();

    // 面包屑
    std::filesystem::path p(s_CurrentPath);
    std::vector<std::filesystem::path> parts;
    auto rel = std::filesystem::relative(p, s_RootPath);
    parts.push_back(std::filesystem::path(s_RootPath));
    for (auto& comp : rel) {
        if (comp != ".") parts.push_back(comp);
    }

    for (size_t i = 0; i < parts.size(); i++) {
        if (i > 0) { ImGui::SameLine(); ImGui::Text(">"); ImGui::SameLine(); }
        std::string label = parts[i].filename().string();
        if (ImGui::SmallButton(label.c_str())) {
            std::filesystem::path navPath = std::filesystem::path(s_RootPath);
            for (size_t j = 1; j <= i; j++) navPath /= parts[j];
            NavigateTo(navPath.string());
        }
    }
}

void AssetBrowser::RenderFileGrid() {
    std::string filter(s_SearchBuffer);
    float padding = 8.0f;
    float cellSize = s_ThumbnailSize + padding;
    float panelWidth = ImGui::GetContentRegionAvail().x;
    int columns = (int)(panelWidth / cellSize);
    if (columns < 1) columns = 1;

    ImGui::BeginChild("AssetGrid", ImVec2(0, 0), false);

    int col = 0;
    for (auto& entry : s_Entries) {
        if (!filter.empty() && entry.Name.find(filter) == std::string::npos) continue;

        ImGui::PushID(entry.FullPath.c_str());

        ImGui::BeginGroup();

        // 图标背景
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 iconSize(s_ThumbnailSize, s_ThumbnailSize * 0.75f);

        ImU32 bgColor = entry.IsDirectory ? IM_COL32(60, 55, 30, 200) : IM_COL32(40, 40, 50, 200);
        dl->AddRectFilled(pos, ImVec2(pos.x + iconSize.x, pos.y + iconSize.y), bgColor, 4.0f);
        dl->AddRect(pos, ImVec2(pos.x + iconSize.x, pos.y + iconSize.y), entry.IconColor, 4.0f);

        // 类型标签
        const char* icon = entry.IsDirectory ? "[DIR]" : GetExtensionIcon(entry.Extension);
        dl->AddText(ImVec2(pos.x + 4, pos.y + iconSize.y * 0.5f - 6),
                    entry.IconColor, icon);

        ImGui::Dummy(iconSize);

        // 文件名 (截断)
        float textWidth = ImGui::CalcTextSize(entry.Name.c_str()).x;
        if (textWidth > s_ThumbnailSize) {
            std::string truncated = entry.Name.substr(0, (size_t)(s_ThumbnailSize / 7.0f)) + "..";
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1), "%s", truncated.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1), "%s", entry.Name.c_str());
        }

        ImGui::EndGroup();

        // 双击导航
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            if (entry.IsDirectory) NavigateTo(entry.FullPath);
        }

        // Tooltip
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", entry.Name.c_str());
            if (!entry.IsDirectory) {
                ImGui::Text("类型: %s", entry.Extension.c_str());
                if (entry.FileSize > 1024 * 1024)
                    ImGui::Text("大小: %.1f MB", entry.FileSize / (1024.0f * 1024.0f));
                else if (entry.FileSize > 1024)
                    ImGui::Text("大小: %.1f KB", entry.FileSize / 1024.0f);
                else
                    ImGui::Text("大小: %llu B", (unsigned long long)entry.FileSize);
            }
            ImGui::EndTooltip();
        }

        ImGui::PopID();

        col++;
        if (col < columns) { ImGui::SameLine(); }
        else col = 0;
    }

    ImGui::EndChild();
}

void AssetBrowser::RenderFileList() {
    std::string filter(s_SearchBuffer);

    ImGui::BeginChild("AssetList", ImVec2(0, 0), false);

    ImGui::Columns(3, "AssetColumns");
    ImGui::Text("名称"); ImGui::NextColumn();
    ImGui::Text("类型"); ImGui::NextColumn();
    ImGui::Text("大小"); ImGui::NextColumn();
    ImGui::Separator();

    for (auto& entry : s_Entries) {
        if (!filter.empty() && entry.Name.find(filter) == std::string::npos) continue;

        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(entry.IconColor), "%s %s",
                           entry.IsDirectory ? "[DIR]" : GetExtensionIcon(entry.Extension),
                           entry.Name.c_str());
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && entry.IsDirectory)
            NavigateTo(entry.FullPath);
        ImGui::NextColumn();

        ImGui::Text("%s", entry.IsDirectory ? "文件夹" : entry.Extension.c_str());
        ImGui::NextColumn();

        if (entry.IsDirectory) ImGui::Text("-");
        else if (entry.FileSize > 1024) ImGui::Text("%.1f KB", entry.FileSize / 1024.0f);
        else ImGui::Text("%llu B", (unsigned long long)entry.FileSize);
        ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::EndChild();
}

} // namespace Engine
