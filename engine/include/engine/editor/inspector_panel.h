#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"

#include <imgui.h>

namespace Engine {

// ── 属性检查器面板 ──────────────────────────────────────────
// 功能:
//   1. Transform 属性编辑 (位置/旋转/缩放 滑块)
//   2. Material 颜色拾取 + PBR 参数
//   3. Render 组件 (Mesh 类型下拉)
//   4. 添加/删除组件
//   5. Tag 重命名
//   6. 所有简单组件的通用编辑

class InspectorPanel {
public:
    static void Init();
    static void Shutdown();

    /// 渲染选中实体的属性面板
    static void Render(ECSWorld& world, Entity selectedEntity);

private:
    static void DrawTransformSection(TransformComponent* tc);
    static void DrawRenderSection(RenderComponent* rc);
    static void DrawMaterialSection(MaterialComponent* mc);
    static void DrawHealthSection(HealthComponent* hc);
    static void DrawVelocitySection(VelocityComponent* vc);
    static void DrawScriptSection(ScriptComponent* sc);
    static void DrawAISection(AIComponent* ai);
    static void DrawTagSection(TagComponent* tag);
    static void DrawAddComponentMenu(ECSWorld& world, Entity entity);

    // 用于 Vec3 拖拽输入的辅助函数
    static bool DrawVec3Control(const char* label, f32& x, f32& y, f32& z,
                                 f32 resetValue = 0.0f, f32 speed = 0.1f);
};

} // namespace Engine
