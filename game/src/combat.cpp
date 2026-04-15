#include "game/combat.h"
#include "game/inventory.h"

#include <cmath>
#include <algorithm>

namespace Engine {

void CombatSystem::Update(ECSWorld& world, f32 dt) {
    // 更新攻击冷却
    world.ForEach<CombatComponent>([&](Entity e, CombatComponent& combat) {
        if (combat.m_cooldownTimer > 0.0f) {
            combat.m_cooldownTimer -= dt;
        }
        if (combat.m_isAttacking) {
            combat.m_attackTimer -= dt;
            if (combat.m_attackTimer <= 0.0f) {
                combat.m_isAttacking = false;
            }
        }
    });

    // 更新投射物
    world.ForEach<ProjectileComponent>([&](Entity e, ProjectileComponent& proj) {
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) return;

        tr->X += proj.m_direction.x * proj.m_speed * dt;
        tr->Y += proj.m_direction.y * proj.m_speed * dt;

        proj.m_lifetime -= dt;
        if (proj.m_lifetime <= 0.0f) {
            world.DestroyEntity(e);
        }
    });

    // 更新掉落物生命周期
    world.ForEach<LootDropComponent>([&](Entity e, LootDropComponent& loot) {
        loot.m_lifetime -= dt;
        if (loot.m_lifetime <= 0.0f) {
            world.DestroyEntity(e);
        }
    });
}

void CombatSystem::MeleeAttack(ECSWorld& world, Entity attacker) {
    auto* combat = world.GetComponent<CombatComponent>(attacker);
    if (!combat || combat.m_cooldownTimer > 0.0f) return;

    auto* tr = world.GetComponent<TransformComponent>(attacker);
    if (!tr) return;

    combat.m_isAttacking = true;
    combat.m_attackTimer = combat.m_attackDuration;
    combat.m_cooldownTimer = combat.m_attackCooldown;

    const glm::vec2 attackerPos = {tr->X, tr->Y};

    // 攻击方向基于玩家朝向 (RotationZ 表示朝向角度)
    const f32 attackAngle = glm::radians(tr->RotZ);
    const glm::vec2 attackDir = {std::cos(attackAngle), std::sin(attackAngle)};

    world.ForEach<HealthComponent>([&](Entity target, HealthComponent& hp) {
        if (target == attacker) return;
        
        auto* ttr = world.GetComponent<TransformComponent>(target);
        if (!ttr) return;

        const glm::vec2 targetPos = {ttr->X, ttr->Y};
        const glm::vec2 diff = targetPos - attackerPos;
        const f32 dist = std::hypot(diff.x, diff.y);

        if (dist > combat.m_attackRange) return;

        // 检查是否在攻击方向的扇形内
        if (dist > k_minAttackDist) {
            const glm::vec2 dirToTarget = diff / dist;
            const f32 dot = glm::dot(attackDir, dirToTarget);
            if (dot < k_cosAttackAngle) return;  // 在背后就不打
        }

        DealDamage(world, target, combat.m_attackDamage,
                   dist > k_minAttackDist ? diff / dist : glm::vec2{0,0},
                   combat.m_knockbackForce);
    });
}

void CombatSystem::DealDamage(ECSWorld& world, Entity target, f32 damage,
                               const glm::vec2& knockDir, f32 knockForce) {
    auto* hp = world.GetComponent<HealthComponent>(target);
    if (!hp) return;

    // 减去防御
    auto* combat = world.GetComponent<CombatComponent>(target);
    f32 actualDamage = combat ? std::max(k_minDamage, damage - combat.m_defense) : damage;

    hp->Current -= actualDamage;
    if (hp->Current < 0.0f) hp->Current = 0.0f;

    // 击退
    if (knockForce > 0.0f && (knockDir.x != 0.0f || knockDir.y != 0.0f)) {
        auto* tr = world.GetComponent<TransformComponent>(target);
        if (tr) {
            tr->X += knockDir.x * knockForce * k_knockbackMult;
            tr->Y += knockDir.y * knockForce * k_knockbackMult;
        }
    }
}

bool CombatSystem::IsDead(ECSWorld& world, Entity e) const {
    auto* hp = world.GetComponent<HealthComponent>(e);
    return hp && hp->Current <= 0.0f;
}

void CombatSystem::SpawnLoot(ECSWorld& world, const glm::vec2& pos,
                              u32 itemID, u32 count) {
    Entity e = world.CreateEntity("Loot");
    auto& tr = world.AddComponent<TransformComponent>(e);
    tr.X = pos.x;
    tr.Y = pos.y;
    tr.SetScale(0.4f);

    auto& loot = world.AddComponent<LootDropComponent>(e);
    loot.m_itemID = itemID;
    loot.m_count = count;
}

void CombatSystem::PickupLoot(ECSWorld& world, Entity player) {
    auto* ptr = world.GetComponent<TransformComponent>(player);
    if (!ptr) return;
    
    const glm::vec2 playerPos = {ptr->X, ptr->Y};

    // 临时收集需要销毁的实体
    std::vector<Entity> toDestroy;
    toDestroy.reserve(8);  // 预分配小缓冲区

    world.ForEach<LootDropComponent>([&](Entity e, LootDropComponent& loot) {
        auto* ltr = world.GetComponent<TransformComponent>(e);
        if (!ltr) return;

        const glm::vec2 lootPos = {ltr->X, ltr->Y};
        const f32 dist = std::hypot(lootPos.x - playerPos.x, lootPos.y - playerPos.y);

        if (dist <= loot.m_pickupRange) {
            // 尝试添加到背包
            if (auto* inv = world.GetComponent<InventoryComponent>(player)) {
                inv->AddItem(loot.m_itemID, loot.m_count);
            }
            toDestroy.push_back(e);
        }
    });

    for (const auto e : toDestroy) {
        world.DestroyEntity(e);
    }
}

} // namespace Engine
