#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"
#include "engine/game2d/sprite2d.h"
#include "engine/platform/input.h"

#include <glm/glm.hpp>
#include <string>

namespace Engine {

// ── 朝向枚举 ──────────────────────────────────────────────

enum class Direction : u8 {
    Down = 0,   // 面向屏幕 (默认)
    Up,
    Left,
    Right,
};

// ── 工具类型 ──────────────────────────────────────────────

enum class ToolType : u8 {
    None = 0,
    Hoe,        // 锄头 — 翻地
    WaterCan,   // 水壶 — 浇水
    Axe,        // 斧头 — 砍树
    Pickaxe,    // 镐   — 碎石
    Scythe,     // 镰刀 — 割草/收割
    FishingRod, // 钓竿
    Seed,       // 种子 (当前持有)
};

// ── 玩家组件 ──────────────────────────────────────────────

struct PlayerComponent : public Component {
    f32       MoveSpeed = 3.5f;        // Tile/秒
    Direction Facing    = Direction::Down;
    ToolType  CurrentTool = ToolType::None;

    bool IsMoving    = false;
    bool IsUsingTool = false;
    f32  ToolTimer   = 0.0f;           // 工具使用动画计时
    f32  ToolCooldown = 0.3f;          // 使用间隔

    f32  Stamina     = 100.0f;
    f32  MaxStamina  = 100.0f;

    /// 获取面前一格的 Tile 偏移
    glm::ivec2 GetFacingOffset() const {
        switch (Facing) {
            case Direction::Up:    return { 0,  1};
            case Direction::Down:  return { 0, -1};
            case Direction::Left:  return {-1,  0};
            case Direction::Right: return { 1,  0};
        }
        return {0, 0};
    }
};

// ── 玩家控制系统 ──────────────────────────────────────────

class PlayerControlSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override;
    const char* GetName() const override { return "PlayerControlSystem"; }
};

} // namespace Engine
