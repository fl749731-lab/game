#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"
#include "engine/ai/behavior_tree.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Engine {

// ── 丧尸类型 ──────────────────────────────────────────────

enum class ZombieType : u8 {
    Walker = 0, // 普通行尸: 慢速, 低攻
    Runner,     // 奔跑者: 快速, 中攻
    Tank,       // 重型: 慢速, 高攻高血, 大体型
};

// ── 丧尸组件 ──────────────────────────────────────────────

struct ZombieComponent : public Component {
    ZombieType Type       = ZombieType::Walker;
    f32 AggroRange        = 8.0f;    // 仇恨范围
    f32 DeaggroRange      = 15.0f;   // 脱战范围
    f32 MoveSpeed         = 1.5f;    // Walker=1.5, Runner=3.5, Tank=1.0
    f32 AttackDamage      = 5.0f;    // 对玩家的伤害
    f32 AttackRange       = 0.8f;    // 攻击距离
    f32 AttackCooldown    = 1.0f;    // 攻击间隔
    f32 CooldownTimer     = 0.0f;
    f32 BuildingDamage    = 2.0f;    // 对建筑的伤害
    u32 XPReward          = 5;       // 击杀奖励经验

    // 寻路
    std::vector<glm::vec3> Path;     // A* 路径点列表
    u32 PathIndex         = 0;
    f32 PathRefreshTimer  = 0.0f;    // 路径刷新计时
    f32 PathRefreshRate   = 1.0f;    // 每秒刷新一次路径

    // 状态
    Entity Target         = INVALID_ENTITY;  // 当前目标
    bool IsAggro          = false;
    f32  WanderTimer      = 0.0f;
    glm::vec2 WanderDir   = {0, 0};
};

// ── 丧尸数据预设 ──────────────────────────────────────────

struct ZombiePreset {
    ZombieType Type;
    f32 Health;
    f32 MoveSpeed;
    f32 AttackDamage;
    f32 AttackRange;
    f32 AggroRange;
    f32 BuildingDamage;
    u32 XPReward;
    f32 Scale;   // 视觉大小
};

inline ZombiePreset GetZombiePreset(ZombieType type) {
    switch (type) {
        case ZombieType::Walker:
            return {ZombieType::Walker, 30.0f, 1.5f, 5.0f, 0.8f, 8.0f, 2.0f, 5, 0.8f};
        case ZombieType::Runner:
            return {ZombieType::Runner, 20.0f, 3.5f, 8.0f, 0.8f, 12.0f, 1.0f, 10, 0.7f};
        case ZombieType::Tank:
            return {ZombieType::Tank, 100.0f, 1.0f, 15.0f, 1.2f, 6.0f, 5.0f, 25, 1.3f};
        default:
            return {ZombieType::Walker, 30.0f, 1.5f, 5.0f, 0.8f, 8.0f, 2.0f, 5, 0.8f};
    }
}

// ── 丧尸系统 ──────────────────────────────────────────────

class NavGrid;  // 前向声明

class ZombieSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override;
    const char* GetName() const override { return "ZombieSystem"; }

    /// 设置寻路网格
    void SetNavGrid(NavGrid* grid) { m_NavGrid = grid; }

    /// 设置玩家实体 (追踪目标)
    void SetPlayerEntity(Entity player) { m_Player = player; }

    /// 生成一只丧尸
    Entity SpawnZombie(ECSWorld& world, const glm::vec2& pos, ZombieType type);

private:
    void UpdateZombieAI(ECSWorld& world, Entity e, ZombieComponent& zombie, f32 dt);

    NavGrid* m_NavGrid = nullptr;
    Entity   m_Player  = INVALID_ENTITY;
};

// ── 丧尸刷新器 ────────────────────────────────────────────

class ZombieSpawner {
public:
    void Update(f32 dt, bool isNight, u32 dayCount);

    /// 检查是否应刷新新一波
    bool ShouldSpawnWave() const { return m_SpawnPending; }
    void ConsumeSpawn() { m_SpawnPending = false; }

    /// 获取当前波数/难度
    u32 GetWaveNumber() const { return m_WaveNumber; }
    u32 GetSpawnCount() const { return m_SpawnCount; }

    // 配置
    void SetWaveInterval(f32 seconds) { m_WaveInterval = seconds; }

private:
    bool m_SpawnPending   = false;
    bool m_WasNight       = false;
    u32  m_WaveNumber     = 0;
    u32  m_SpawnCount     = 3;      // 初始每波3只
    f32  m_WaveTimer      = 0.0f;
    f32  m_WaveInterval   = 30.0f;  // 夜间每30秒一波
};

} // namespace Engine
