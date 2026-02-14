#include "engine/editor/hierarchy_panel.h"
#include "engine/core/log.h"

#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace Engine {

void HierarchyPanel::Init() {
    s_SelectedEntity = INVALID_ENTITY;
    s_SelectedEntities.clear();
    LOG_INFO("[HierarchyPanel] 初始化");
}

void HierarchyPanel::Shutdown() {
    s_SelectedEntities.clear();
    LOG_INFO("[HierarchyPanel] 关闭");
}

void HierarchyPanel::ClearSelection() {
    s_SelectedEntity = INVALID_ENTITY;
    s_SelectedEntities.clear();
}

ImU32 HierarchyPanel::GetEntityIconColor(ECSWorld& world, Entity entity) {
    if (world.HasComponent<MaterialComponent>(entity))
        return IM_COL32(100, 200, 255, 255);  // 有材质 — 蓝
    if (world.HasComponent<RenderComponent>(entity))
        return IM_COL32(80, 255, 80, 255);    // 有渲染 — 绿
    if (world.HasComponent<ScriptComponent>(entity))
        return IM_COL32(255, 200, 80, 255);   // 有脚本 — 黄
    return IM_COL32(180, 180, 180, 255);       // 默认 — 灰
}

const char* HierarchyPanel::GetEntityIcon(ECSWorld& world, Entity entity) {
    if (world.HasComponent<RenderComponent>(entity)) {
        auto* rc = world.GetComponent<RenderComponent>(entity);
        if (rc && rc->MeshType == "sphere") return "[S]";
        if (rc && rc->MeshType == "plane")  return "[P]";
        return "[M]";  // Mesh
    }
    if (world.HasComponent<ScriptComponent>(entity)) return "[SC]";
    return "[E]";  // Entity
}

void HierarchyPanel::Render(ECSWorld& world) {
    if (!ImGui::Begin("层级##Hierarchy")) { ImGui::End(); return; }

    RenderSearchBar();
    ImGui::Separator();

    // 实体列表
    ImGui::BeginChild("EntityList", ImVec2(0, -30), false);

    std::string filter(s_SearchBuffer);

    // 遍历所有实体
    for (Entity e : world.GetEntities()) {
        auto* tag = world.GetComponent<TagComponent>(e);
        if (!tag) continue;

        // 搜索过滤
        if (!filter.empty() && tag->Name.find(filter) == std::string::npos)
            continue;

        RenderEntityNode(world, e);
    }

    ImGui::EndChild();

    // 右键菜单
    RenderContextMenu(world);

    // 底部信息
    ImGui::Separator();
    ImGui::Text("实体: %u | 选中: %u",
                (u32)world.GetEntities().size(),
                s_SelectedEntity != INVALID_ENTITY ? 1u : 0u);

    ImGui::End();
}

void HierarchyPanel::RenderEntityNode(ECSWorld& world, Entity entity) {
    auto* tag = world.GetComponent<TagComponent>(entity);
    if (!tag) return;

    bool isSelected = (s_SelectedEntity == entity);
    ImU32 iconColor = GetEntityIconColor(world, entity);
    const char* icon = GetEntityIcon(world, entity);

    // 树节点 (叶子)
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
                                ImGuiTreeNodeFlags_SpanAvailWidth |
                                ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(iconColor));
    ImGui::Text("%s", icon);
    ImGui::PopStyleColor();
    ImGui::SameLine();

    ImGui::TreeNodeEx((void*)(intptr_t)entity, flags, "%s", tag->Name.c_str());

    // 点击选中
    if (ImGui::IsItemClicked(0)) {
        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl) {
            // Ctrl 多选
            auto it = std::find(s_SelectedEntities.begin(), s_SelectedEntities.end(), entity);
            if (it != s_SelectedEntities.end())
                s_SelectedEntities.erase(it);
            else
                s_SelectedEntities.push_back(entity);
        }
        s_SelectedEntity = entity;
    }

    // 右键
    if (ImGui::IsItemClicked(1)) {
        s_SelectedEntity = entity;
        s_ShowContextMenu = true;
    }

    // 拖拽源
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload("ENTITY_DRAG", &entity, sizeof(Entity));
        ImGui::Text("移动: %s", tag->Name.c_str());
        ImGui::EndDragDropSource();
    }

    // 拖拽目标
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG")) {
            Entity srcEntity = *(Entity*)payload->Data;
            // 设置父子关系（TransformComponent.Parent）
            auto* srcTransform = world.GetComponent<TransformComponent>(srcEntity);
            if (srcTransform) {
                srcTransform->Parent = entity;
                LOG_INFO("[Hierarchy] 设置 %u 的父节点为 %u", srcEntity, entity);
            }
        }
        ImGui::EndDragDropTarget();
    }

    // 双击重命名
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        // TODO: 内联重命名编辑
    }
}

void HierarchyPanel::RenderContextMenu(ECSWorld& world) {
    if (s_ShowContextMenu) {
        ImGui::OpenPopup("##HierarchyContext");
        s_ShowContextMenu = false;
    }

    if (ImGui::BeginPopup("##HierarchyContext")) {
        if (ImGui::MenuItem("创建空实体")) {
            Entity e = world.CreateEntity("新实体");
            world.AddComponent<TransformComponent>(e);
            s_SelectedEntity = e;
        }

        if (ImGui::BeginMenu("创建预设")) {
            if (ImGui::MenuItem("立方体")) {
                Entity e = world.CreateEntity("Cube");
                world.AddComponent<TransformComponent>(e);
                world.AddComponent<RenderComponent>(e).MeshType = "cube";
                world.AddComponent<MaterialComponent>(e);
                s_SelectedEntity = e;
            }
            if (ImGui::MenuItem("球体")) {
                Entity e = world.CreateEntity("Sphere");
                world.AddComponent<TransformComponent>(e);
                world.AddComponent<RenderComponent>(e).MeshType = "sphere";
                world.AddComponent<MaterialComponent>(e);
                s_SelectedEntity = e;
            }
            if (ImGui::MenuItem("平面")) {
                Entity e = world.CreateEntity("Plane");
                world.AddComponent<TransformComponent>(e);
                world.AddComponent<RenderComponent>(e).MeshType = "plane";
                world.AddComponent<MaterialComponent>(e);
                s_SelectedEntity = e;
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (s_SelectedEntity != INVALID_ENTITY) {
            if (ImGui::MenuItem("删除")) {
                world.DestroyEntity(s_SelectedEntity);
                s_SelectedEntity = INVALID_ENTITY;
            }
            if (ImGui::MenuItem("复制")) {
                auto* tag = world.GetComponent<TagComponent>(s_SelectedEntity);
                Entity dup = world.CreateEntity(tag ? tag->Name + " (副本)" : "副本");
                world.AddComponent<TransformComponent>(dup);
                auto* srcTf = world.GetComponent<TransformComponent>(s_SelectedEntity);
                if (srcTf) {
                    auto* dstTf = world.GetComponent<TransformComponent>(dup);
                    *dstTf = *srcTf;
                    dstTf->X += 1.0f;  // 偏移避免重叠
                }
                s_SelectedEntity = dup;
            }
        }

        ImGui::EndPopup();
    }
}

void HierarchyPanel::RenderSearchBar() {
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##HierSearch", "搜索实体...", s_SearchBuffer, sizeof(s_SearchBuffer));
}

} // namespace Engine
