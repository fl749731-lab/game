#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"

#include <imgui.h>
#include <string>
#include <functional>

namespace Engine {

// ── 场景层级面板 ────────────────────────────────────────────
// Unity/UE 风格的场景实体树形视图
// 功能:
//   1. 树形展开/折叠
//   2. 单击选中 / Ctrl 多选
//   3. 拖拽改变父子关系
//   4. 右键菜单 (创建/删除/重命名)
//   5. 搜索过滤
//   6. 实体图标颜色 (按组件类型)

class HierarchyPanel {
public:
    static void Init();
    static void Shutdown();

    /// 渲染层级面板
    static void Render(ECSWorld& world);

    /// 选中实体
    static Entity GetSelectedEntity() { return s_SelectedEntity; }
    static void SetSelectedEntity(Entity e) { s_SelectedEntity = e; }

    /// 多选
    static const std::vector<Entity>& GetSelectedEntities() { return s_SelectedEntities; }
    static void ClearSelection();

    /// 创建实体回调
    using CreateCallback = std::function<void(ECSWorld&, const std::string&)>;
    static void SetCreateCallback(CreateCallback cb) { s_CreateCallback = cb; }

private:
    static void RenderEntityNode(ECSWorld& world, Entity entity);
    static void RenderContextMenu(ECSWorld& world);
    static void RenderSearchBar();

    static ImU32 GetEntityIconColor(ECSWorld& world, Entity entity);
    static const char* GetEntityIcon(ECSWorld& world, Entity entity);

    inline static Entity s_SelectedEntity = INVALID_ENTITY;
    inline static std::vector<Entity> s_SelectedEntities;
    inline static char s_SearchBuffer[256] = {};
    inline static bool s_ShowContextMenu = false;
    inline static CreateCallback s_CreateCallback;
    inline static Entity s_DragSourceEntity = INVALID_ENTITY;
};

} // namespace Engine
