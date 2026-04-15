#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"
#include "engine/ai/behavior_tree.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <array>

namespace Engine {

// ── 丧尸类型 ──────────────────────────────────────────────

enum class ZombieType : u8 {
    Walker = 0, // 普通行尸: 慢速, 低攻
    Runner,     // 奔跑者: 快速, 中攻
    Tank,       // 重型: 慢速, 高攻高血, 大体型

    Count  // 用于数组大小
};

// ── 丧尸数据预设 ──────────────────────────────────────────

struct ZombiePreset {
    f32 m_health;
    f32 m_moveSpeed;
    f32 m_attackDamage;
    f32 m_attackRange;
    f32 m_aggroRange;
    f32 m_buildingDamage;
    u32 m_xpReward;
    f32 m_scale;   // 视觉大小
};

// 丧尸预设查找表
[[nodiscard]] inline const ZombiePreset& GetZombiePreset(ZombieType type) {
    static constexpr std::array<ZombiePreset, static_cast<size_t>(ZombieType::Count)> s_presets = {{
        {30.0f,  1.5f,  5.0f,  0.8f,  8.0f,  2.0f,  5,  0.8f},   // Walker
        {20.0f,  3.5f,  8.0f,  0.8f,  12.0f, 1.0f,  10, 0.7f},   // Runner
        {100.0f, 1.0f,  15.0f, 1.2f,  6.0f,  5.0f,  25, 1.3f}    // Tank
    }};
    const size_t idx = static_cast<size_t>(type);
    return (idx < s_presets.size()) ? s_presets[idx] : s_presets[0];
}

// 丧尸名称查找表
[[nodiscard]] inline const char* GetZombieTypeName(ZombieType type) {
    static constexpr std::array<const char*, static_cast<size_t>(ZombieType::Count)> s_names = {
        "Zombie_Walker", "Zombie_Runner", "Zombie_Tank"
    };
    const size_t idx = static_cast<size_t>(type);
    return (idx < s_names.size()) ? s_names[idx] : "Zombie_Unknown";
}

// ── 丧尸组件 ──────────────────────────────────────────────

struct ZombieComponent : public Component {
    ZombieType m_type       = ZombieType::Walker;
    f32 m_aggroRange        = 8.0f;    // 仇恨范围
    f32 m_deaggroRange      = 15.0f;   // 脱战范围
    f32 m_moveSpeed         = 1.5f;    // Walker=1.5, Runner=3.5, Tank=1.0
    f32 m_attackDamage      = 5.0f;    // 对玩家的伤害
    f32 m_attackRange       = 0.8f;    // 攻击距离
    f32 m_attackCooldown    = 1.0f;    // 攻击间隔
    f32 m_cooldownTimer     = 0.0f;
    f32 m_buildingDamage    = 2.0f;    // 对建筑的伤害
    u32 m_xpReward          = 5;       // 击杀奖励经验

    // 寻路
    std::vector<glm::vec3> m_path;     // A* 路径点列表
    u32 m_pathIndex         = 0;
    f32 m_pathRefreshTimer  = 0.0f;    // 路径刷新计时
    f32 m_pathRefreshRate   = 1.0f;    // 每秒刷新一次路径

    // 状态
    Entity m_target         = INVALID_ENTITY;  // 当前目标
    bool m_isAggro          = false;
    f32  m_wanderTimer      = 0.0f;
    glm::vec2 m_wanderDir   = {0, 0};
};

// ── 丧尸系统 ──────────────────────────────────────────────

class NavGrid;  // 前向声明

class ZombieSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override;
    [[nodiscard]] const char* GetName() const override { return "ZombieSystem"; }

    /// 设置寻路网格
    void SetNavGrid(NavGrid* grid) { m_navGrid = grid; }

    /// 设置玩家实体 (追踪目标)
    void SetPlayerEntity(Entity player) { m_player = player; }

    /// 生成一只丧尸
    [[nodiscard]] Entity SpawnZombie(ECSWorld& world, const glm::vec2& pos, ZombieType type);

private:
    void UpdateZombieAI(ECSWorld& world, Entity e, ZombieComponent& zombie, f32 dt);

    // AI 常量
    static constexpr f32 k_pathNodeReachedDist = 0.3f;      // 到达路径点判定距离
    static constexpr f32 k_minMoveDist = 0.1f;              // 最小移动距离（防止抖动）
    static constexpr f32 k_wanderTimerMin = 2.0f;             // 游荡计时器最小值
    static constexpr f32 k_wanderTimerMax = 5.0f;             // 游荡计时器最大值
    static constexpr f32 k_wanderSpeedMult = 0.3f;          // 游荡速度系数
    static constexpr f32 k_noPathDistThreshold = 9999.0f;   // 无玩家时的默认距离

    NavGrid* m_navGrid = nullptr;
    Entity   m_player  = INVALID_ENTITY;
};

// ── 丧尸刷新器 ────────────────────────────────────────────

class ZombieSpawner {
public:
    void Update(f32 dt, bool isNight, u32 dayCount);

    /// 检查是否应刷新新一波
    [[nodiscard]] bool ShouldSpawnWave() const { return m_spawnPending; }
    void ConsumeSpawn() { m_spawnPending = false; }

    /// 获取当前波数/难度
    [[nodiscard]] u32 GetWaveNumber() const { return m_waveNumber; }
    [[nodiscard]] u32 GetSpawnCount() const { return m_spawnCount; }

    // 配置
    void SetWaveInterval(f32 seconds) { m_waveInterval = seconds; }

private:
    // 刷怪常量
    static constexpr u32 k_baseSpawnCount = 3;              // 基础每波数量
    static constexpr u32 k_spawnIncreasePerDay = 2;       // 每天增加数量
    static constexpr u32 k_spawnIncreasePerWave = 2;        // 每波增加数量

    bool m_spawnPending   = false;
    bool m_wasNight       = false;
    u32  m_waveNumber     = 0;
    u32  m_spawnCount     = k_baseSpawnCount;
    f32  m_waveTimer      = 0.0f;
    f32  m_waveInterval   = 30.0f;  // 夜间每30秒一波
};

} // namespace Engine
