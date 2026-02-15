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
    if (!tr || !hp || hp->Current <= 0) return;

    glm::vec2 zombiePos = {tr->X, tr->Y};
    glm::vec2 playerPos = {0, 0};
    f32 distToPlayer = 9999.0f;

    // 获取玩家位置
    if (m_Player != INVALID_ENTITY) {
        auto* ptr = world.GetComponent<TransformComponent>(m_Player);
        if (ptr) {
            playerPos = {ptr->X, ptr->Y};
            f32 dx = playerPos.x - zombiePos.x;
            f32 dy = playerPos.y - zombiePos.y;
            distToPlayer = std::sqrt(dx * dx + dy * dy);
        }
    }

    // 更新攻击冷却
    if (zombie.CooldownTimer > 0) zombie.CooldownTimer -= dt;

    // ── AI 决策 ──────────────────────────────────────────

    // 1) 仇恨检测
    if (!zombie.IsAggro && distToPlayer < zombie.AggroRange) {
        zombie.IsAggro = true;
        zombie.Target = m_Player;
    }
    if (zombie.IsAggro && distToPlayer > zombie.DeaggroRange) {
        zombie.IsAggro = false;
        zombie.Target = INVALID_ENTITY;
        zombie.Path.clear();
    }

    if (zombie.IsAggro && zombie.Target != INVALID_ENTITY) {
        // 2) 在攻击范围内 → 攻击
        if (distToPlayer <= zombie.AttackRange) {
            if (zombie.CooldownTimer <= 0) {
                zombie.CooldownTimer = zombie.AttackCooldown;
                // 对玩家造成伤害
                auto* playerHP = world.GetComponent<HealthComponent>(m_Player);
                if (playerHP) {
                    playerHP->Current -= zombie.AttackDamage;
                    if (playerHP->Current < 0) playerHP->Current = 0;
                }
            }
            return;  // 攻击时不移动
        }

        // 3) A* 寻路追击
        zombie.PathRefreshTimer -= dt;
        if (zombie.PathRefreshTimer <= 0 && m_NavGrid) {
            zombie.PathRefreshTimer = zombie.PathRefreshRate;
            glm::vec3 start = {zombiePos.x, zombiePos.y, 0};
            glm::vec3 goal  = {playerPos.x, playerPos.y, 0};
            zombie.Path = m_NavGrid->FindPath(start, goal);
            zombie.PathIndex = 0;
        }

        // 沿路径移动
        if (!zombie.Path.empty() && zombie.PathIndex < zombie.Path.size()) {
            glm::vec2 target2d = {zombie.Path[zombie.PathIndex].x,
                                   zombie.Path[zombie.PathIndex].y};
            glm::vec2 diff = target2d - zombiePos;
            f32 dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);

            if (dist < 0.3f) {
                zombie.PathIndex++;
            } else {
                glm::vec2 dir = diff / dist;
                tr->X += dir.x * zombie.MoveSpeed * dt;
                tr->Y += dir.y * zombie.MoveSpeed * dt;
                // 朝向
                tr->RotZ = std::atan2(dir.y, dir.x);
            }
        } else {
            // 无路径，直接朝玩家移动
            if (distToPlayer > 0.1f) {
                glm::vec2 diff = playerPos - zombiePos;
                glm::vec2 dir = diff / distToPlayer;
                tr->X += dir.x * zombie.MoveSpeed * dt;
                tr->Y += dir.y * zombie.MoveSpeed * dt;
                tr->RotZ = std::atan2(dir.y, dir.x);
            }
        }
    } else {
        // 4) 游荡
        zombie.WanderTimer -= dt;
        if (zombie.WanderTimer <= 0) {
            zombie.WanderTimer = 2.0f + (std::rand() % 30) * 0.1f;
            f32 angle = (std::rand() % 360) * 3.14159f / 180.0f;
            zombie.WanderDir = {std::cos(angle), std::sin(angle)};
        }

        f32 wanderSpeed = zombie.MoveSpeed * 0.3f;
        tr->X += zombie.WanderDir.x * wanderSpeed * dt;
        tr->Y += zombie.WanderDir.y * wanderSpeed * dt;
        tr->RotZ = std::atan2(zombie.WanderDir.y, zombie.WanderDir.x);
    }
}

Entity ZombieSystem::SpawnZombie(ECSWorld& world, const glm::vec2& pos,
                                  ZombieType type) {
    auto preset = GetZombiePreset(type);
    std::string name;
    switch (type) {
        case ZombieType::Walker: name = "Zombie_Walker"; break;
        case ZombieType::Runner: name = "Zombie_Runner"; break;
        case ZombieType::Tank:   name = "Zombie_Tank";   break;
    }

    Entity e = world.CreateEntity(name);

    auto& tr = world.AddComponent<TransformComponent>(e);
    tr.X = pos.x;
    tr.Y = pos.y;
    tr.ScaleX = tr.ScaleY = tr.ScaleZ = preset.Scale;

    auto& hp = world.AddComponent<HealthComponent>(e);
    hp.Current = preset.Health;
    hp.Max = preset.Health;

    auto& zombie = world.AddComponent<ZombieComponent>(e);
    zombie.Type           = type;
    zombie.MoveSpeed      = preset.MoveSpeed;
    zombie.AttackDamage   = preset.AttackDamage;
    zombie.AttackRange    = preset.AttackRange;
    zombie.AggroRange     = preset.AggroRange;
    zombie.BuildingDamage = preset.BuildingDamage;
    zombie.XPReward       = preset.XPReward;

    return e;
}

// ════════════════════════════════════════════════════════════
//  ZombieSpawner
// ════════════════════════════════════════════════════════════

void ZombieSpawner::Update(f32 dt, bool isNight, u32 dayCount) {
    // 日夜切换检测
    if (isNight && !m_WasNight) {
        // 刚进入夜晚 → 立即触发第一波
        m_WasNight = true;
        m_WaveNumber = 0;
        m_WaveTimer = 0;
        m_SpawnCount = 3 + dayCount * 2;  // 每天增加2只基础量
        m_SpawnPending = true;
        m_WaveNumber++;
    }
    if (!isNight && m_WasNight) {
        m_WasNight = false;
    }

    // 夜间持续刷新
    if (isNight) {
        m_WaveTimer += dt;
        if (m_WaveTimer >= m_WaveInterval) {
            m_WaveTimer = 0;
            m_WaveNumber++;
            m_SpawnCount = (3 + dayCount * 2) + m_WaveNumber * 2;
            m_SpawnPending = true;
        }
    }
}

} // namespace Engine
