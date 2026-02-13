#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"

#include <string>
#include <vector>

namespace Engine {

// ── 脚本系统 ────────────────────────────────────────────────
// 遍历所有 ScriptComponent，调用 Python 脚本的标准生命周期：
//   on_create(entity_id)        — 首次执行时调用
//   on_update(entity_id, dt)    — 每帧调用
//   on_event(entity_id, event)  — 事件触发时调用
//   on_destroy(entity_id)       — 销毁前调用

class ScriptSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override;
    const char* GetName() const override { return "ScriptSystem"; }

    /// 向指定脚本组件发送事件
    static void SendEvent(ECSWorld& world, Entity e,
                          const std::string& eventType,
                          const std::string& eventData = "");

    /// 销毁实体前调用其 on_destroy
    static void NotifyDestroy(ECSWorld& world, Entity e);

private:
    /// 构建实体的上下文 JSON (位置、组件数据等)
    static std::string BuildEntityContext(ECSWorld& world, Entity e);
};

} // namespace Engine
