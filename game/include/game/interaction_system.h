#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"
#include "engine/game2d/tilemap.h"

#include <glm/glm.hpp>
#include <string>
#include <functional>

namespace Engine {

// ── 交互事件 ──────────────────────────────────────────────

struct InteractionEvent {
    Entity      Source;                  // 交互发起者 (玩家)
    glm::ivec2  TargetTile;             // 目标 Tile 坐标
    u8          InteractType;           // Tile 上的交互类型
    Entity      TargetEntity = 0;       // 目标 Entity (如果有)
};

using InteractionCallback = std::function<void(const InteractionEvent&)>;

// ── 可交互组件 ────────────────────────────────────────────

struct InteractableComponent : public Component {
    std::string PromptText = "交互";    // 提示文字
    u8          Type = 1;               // 自定义类型
    f32         Range = 1.5f;           // 交互距离 (Tile)
};

// ── 场景传送组件 ──────────────────────────────────────────

struct ScenePortalComponent : public Component {
    std::string TargetScene;            // 目标场景文件
    glm::vec2   SpawnPosition = {0,0};  // 传送后出生点 (Tile 坐标)
};

// ── 交互系统 ──────────────────────────────────────────────

class InteractionSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override;
    const char* GetName() const override { return "InteractionSystem"; }

    /// 注册交互回调
    void OnInteraction(InteractionCallback cb) { m_Callbacks.push_back(cb); }

private:
    std::vector<InteractionCallback> m_Callbacks;
};

} // namespace Engine
