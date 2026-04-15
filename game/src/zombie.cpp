#include "game/zombie.h"
#include "engine/ai/behavior_tree.h"

#include <cmath>
#include <cstdlib>

namespace Engine {

// ════════════════════════════════════════════════════════════
//  ZombieSystem
// ════════════════════════════════════════════════════════════

void ZombieSystem::Update(ECSWorld& world, f32 dt) {
    world.ForEach<ZombieComponent>([&](Entity e, ZombieComponent& zombie) {
        UpdateZombieAI(world, e, zombie, dt);
    });
}

void ZombieSystem::UpdateZombieAI(ECSWorld& world, Entity e,
                                   ZombieComponent& zombie, f32 dt) {
    auto* tr = world.GetComponent<TransformComponent>(e);
    auto* hp = world.GetComponent<HealthComponent>(e);
    if (!tr || !hp || hp->Current <= 0.0f) return;

    const glm::vec2 zombiePos = {tr->X, tr->Y};
    glm::vec2 playerPos = {0, 0};
    f32 distToPlayer = k_noPathDistThreshold;

    // 获取玩家位置
    if (m_player != INVALID_ENTITY) {
        if (auto* ptr = world.GetComponent<TransformComponent>(m_player)) {
            playerPos = {ptr->X, ptr->Y};
            distToPlayer = std::hypot(playerPos.x - zombiePos.x, 
                                      playerPos.y - zombiePos.y);
        }
    }

    // 更新攻击冷却
    if (zombie.m_cooldownTimer > 0.0f) {
        zombie.m_cooldownTimer -= dt;
    }

    // ── AI 决策 ──────────────────────────────────────────

    // 1) 仇恨检测
    if (!zombie.m_isAggro && distToPlayer < zombie.m_aggroRange) {
        zombie.m_isAggro = true;
        zombie.m_target = m_player;
    }
    if (zombie.m_isAggro && distToPlayer > zombie.m_deaggroRange) {
        zombie.m_isAggro = false;
        zombie.m_target = INVALID_ENTITY;
        zombie.m_path.clear();
    }

    if (zombie.m_isAggro && zombie.m_target != INVALID_ENTITY) {
        // 2) 在攻击范围内 → 攻击
        if (distToPlayer <= zombie.m_attackRange) {
            if (zombie.m_cooldownTimer <= 0.0f) {
                zombie.m_cooldownTimer = zombie.m_attackCooldown;
                // 对玩家造成伤害
                if (auto* playerHP = world.GetComponent<HealthComponent>(m_player)) {
                    playerHP->Current -= zombie.m_attackDamage;
                    if (playerHP->Current < 0.0f) playerHP->Current = 0.0f;
                }
            }
            return;  // 攻击时不移动
        }

        // 3) A* 寻路追击
        zombie.m_pathRefreshTimer -= dt;
        if (zombie.m_pathRefreshTimer <= 0.0f && m_navGrid) {
            zombie.m_pathRefreshTimer = zombie.m_pathRefreshRate;
            const glm::vec3 start = {zombiePos.x, zombiePos.y, 0};
            const glm::vec3 goal  = {playerPos.x, playerPos.y, 0};
            zombie.m_path = m_navGrid->FindPath(start, goal);
            zombie.m_pathIndex = 0;
        }

        // 沿路径移动
        if (!zombie.m_path.empty() && zombie.m_pathIndex < zombie.m_path.size()) {
            const glm::vec2 target2d = {zombie.m_path[zombie.m_pathIndex].x,
                                       zombie.m_path[zombie.m_pathIndex].y};
            const glm::vec2 diff = target2d - zombiePos;
            const f32 dist = std::hypot(diff.x, diff.y);

            if (dist < k_pathNodeReachedDist) {
                zombie.m_pathIndex++;
            } else {
                const glm::vec2 dir = diff / dist;
                tr->X += dir.x * zombie.m_moveSpeed * dt;
                tr->Y += dir.y * zombie.m_moveSpeed * dt;
                tr->RotZ = glm::degrees(std::atan2(dir.y, dir.x));
            }
        } else {
            // 无路径，直接朝玩家移动
            if (distToPlayer > k_minMoveDist) {
                const glm::vec2 diff = playerPos - zombiePos;
                const glm::vec2 dir = diff / distToPlayer;
                tr->X += dir.x * zombie.m_moveSpeed * dt;
                tr->Y += dir.y * zombie.m_moveSpeed * dt;
                tr->RotZ = glm::degrees(std::atan2(dir.y, dir.x));
            }
        }
    } else {
        // 4) 游荡
        zombie.m_wanderTimer -= dt;
        if (zombie.m_wanderTimer <= 0.0f) {
            zombie.m_wanderTimer = k_wanderTimerMin + 
                                   static_cast<f32>(std::rand() % 30) * 0.1f;
            const f32 angle = static_cast<f32>(std::rand() % 360) * 
                              glm::pi<f32>() / 180.0f;
            zombie.m_wanderDir = {std::cos(angle), std::sin(angle)};
        }

        const f32 wanderSpeed = zombie.m_moveSpeed * k_wanderSpeedMult;
        tr->X += zombie.m_wanderDir.x * wanderSpeed * dt;
        tr->Y += zombie.m_wanderDir.y * wanderSpeed * dt;
        tr->RotZ = glm::degrees(std::atan2(zombie.m_wanderDir.y, zombie.m_wanderDir.x));
    }
}

Entity ZombieSystem::SpawnZombie(ECSWorld& world, const glm::vec2& pos,
                                  ZombieType type) {
    const auto& preset = GetZombiePreset(type);
    const char* name = GetZombieTypeName(type);

    Entity e = world.CreateEntity(name);

    auto& tr = world.AddComponent<TransformComponent>(e);
    tr.X = pos.x;
    tr.Y = pos.y;
    tr.SetScale(preset.m_scale);

    auto& hp = world.AddComponent<HealthComponent>(e);
    hp.Current = preset.m_health;
    hp.Max = preset.m_health;

    auto& zombie = world.AddComponent<ZombieComponent>(e);
    zombie.m_type           = type;
    zombie.m_moveSpeed      = preset.m_moveSpeed;
    zombie.m_attackDamage   = preset.m_attackDamage;
    zombie.m_attackRange    = preset.m_attackRange;
    zombie.m_aggroRange     = preset.m_aggroRange;
    zombie.m_buildingDamage = preset.m_buildingDamage;
    zombie.m_xpReward       = preset.m_xpReward;

    return e;
}

// ════════════════════════════════════════════════════════════
//  ZombieSpawner
// ════════════════════════════════════════════════════════════

void ZombieSpawner::Update(f32 dt, bool isNight, u32 dayCount) {
    // 日夜切换检测
    if (isNight && !m_wasNight) {
        // 刚进入夜晚 → 立即触发第一波
        m_wasNight = true;
        m_waveNumber = 0;
        m_waveTimer = 0.0f;
        m_spawnCount = k_baseSpawnCount + dayCount * k_spawnIncreasePerDay;
        m_spawnPending = true;
        m_waveNumber++;
    }
    if (!isNight && m_wasNight) {
        m_wasNight = false;
    }

    // 夜间持续刷新
    if (isNight) {
        m_waveTimer += dt;
        if (m_waveTimer >= m_waveInterval) {
            m_waveTimer = 0.0f;
            m_waveNumber++;
            m_spawnCount = (k_baseSpawnCount + dayCount * k_spawnIncreasePerDay) + 
                           m_waveNumber * k_spawnIncreasePerWave;
            m_spawnPending = true;
        }
    }
}

} // namespace Engine
