#include "engine/editor/drag_drop.h"
#include "engine/core/log.h"

#include <imgui.h>
#include <cstring>
#include <filesystem>
#include <unordered_map>

namespace Engine {

void DragDropManager::Init() {
    s_Handlers.clear();
    LOG_INFO("[DragDrop] 初始化");
}

void DragDropManager::Shutdown() {
    s_Handlers.clear();
    LOG_INFO("[DragDrop] 关闭");
}

bool DragDropManager::BeginSource(const char* type, const DragDropPayload& payload) {
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload(type, &payload, sizeof(DragDropPayload));
        s_CurrentPayload = payload;
        return true;
    }
    return false;
}

void DragDropManager::EndSource() {
    ImGui::EndDragDropSource();
}

const DragDropPayload* DragDropManager::AcceptTarget(const char* type) {
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(type)) {
            ImGui::EndDragDropTarget();
            return (const DragDropPayload*)payload->Data;
        }
        ImGui::EndDragDropTarget();
    }
    return nullptr;
}

void DragDropManager::RegisterFileHandler(const std::string& extension, FileHandler handler) {
    s_Handlers[extension] = handler;
    LOG_INFO("[DragDrop] 注册处理器: .%s", extension.c_str());
}

void DragDropManager::HandleFileDrop(const std::string& path) {
    std::filesystem::path fp(path);
    std::string ext = fp.extension().string();

    // 移除前导点
    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);

    auto it = s_Handlers.find(ext);
    if (it != s_Handlers.end()) {
        it->second(path);
        LOG_INFO("[DragDrop] 处理文件: %s", path.c_str());
    } else {
        LOG_WARN("[DragDrop] 无处理器: .%s", ext.c_str());
    }
}

void DragDropManager::RenderDragPreview() {
    if (s_CurrentPayload.PayloadType == DragDropPayload::AssetPath) {
        ImGui::BeginTooltip();
        std::filesystem::path fp(s_CurrentPayload.Path);
        ImGui::Text("📁 %s", fp.filename().string().c_str());
        ImGui::EndTooltip();
    } else if (s_CurrentPayload.PayloadType == DragDropPayload::EntityRef) {
        ImGui::BeginTooltip();
        ImGui::Text("🔶 实体 #%u", s_CurrentPayload.EntityID);
        ImGui::EndTooltip();
    }
}

} // namespace Engine
