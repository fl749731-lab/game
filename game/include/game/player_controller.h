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

    Count  // 用于数组大小
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

    Count  // 用于数组大小
};

// ── 玩家组件 ──────────────────────────────────────────────

struct PlayerComponent : public Component {
    f32       m_moveSpeed = 3.5f;        // Tile/秒
    Direction m_facing    = Direction::Down;
    ToolType  m_currentTool = ToolType::None;

    bool m_isMoving    = false;
    bool m_isUsingTool = false;
    f32  m_toolTimer   = 0.0f;           // 工具使用动画计时
    f32  m_toolCooldown = 0.3f;          // 使用间隔

    f32  m_stamina     = 100.0f;
    f32  m_maxStamina  = 100.0f;

    /// 获取面前一格的 Tile 偏移
    [[nodiscard]] glm::ivec2 GetFacingOffset() const {
        static constexpr glm::ivec2 s_offsets[] = {
            { 0, -1},  // Down
            { 0,  1},  // Up
            {-1,  0},  // Left
            { 1,  0}   // Right
        };
        static_assert(sizeof(s_offsets) / sizeof(s_offsets[0]) == static_cast<size_t>(Direction::Count),
                      "Direction enum and offset array mismatch");
        return s_offsets[static_cast<size_t>(m_facing)];
    }
};

// ── 玩家控制系统 ──────────────────────────────────────────

class PlayerControlSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override;
    [[nodiscard]] const char* GetName() const override { return "PlayerControlSystem"; }

private:
    // 动画名称映射表：索引 [Direction][IsMoving]
    // Direction: Down, Up, Left, Right
    // IsMoving: 0=idle, 1=walk
    static constexpr const char* s_animationNames[4][2] = {
        {"idle_down",  "walk_down" },   // Down
        {"idle_up",    "walk_up"   },   // Up
        {"idle_left",  "walk_left" },   // Left
        {"idle_right", "walk_right"}    // Right
    };

    static constexpr size_t GetDirectionIndex(Direction dir) {
        return static_cast<size_t>(dir);
    }
};

} // namespace Engine
