#include "game/combat.h"
#include "game/inventory.h"

#include <cmath>
#include <algorithm>

namespace Engine {

void CombatSystem::Update(ECSWorld& world, f32 dt) {
    // 更新攻击冷却
    world.ForEach<CombatComponent>([&](Entity e, CombatComponent& combat) {
        if (combat.CooldownTimer > 0) combat.CooldownTimer -= dt;
        if (combat.IsAttacking) {
            combat.AttackTimer -= dt;
            if (combat.AttackTimer <= 0) combat.IsAttacking = false;
        }
    });

    // 更新投射物
    world.ForEach<ProjectileComponent>([&](Entity e, ProjectileComponent& proj) {
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) return;

        tr->X += proj.Direction.x * proj.Speed * dt;
        tr->Y += proj.Direction.y * proj.Speed * dt;

        proj.Lifetime -= dt;
        if (proj.Lifetime <= 0) {
            world.DestroyEntity(e);
        }
    });

    // 更新掉落物生命周期
    world.ForEach<LootDropComponent>([&](Entity e, LootDropComponent& loot) {
        loot.Lifetime -= dt;
        if (loot.Lifetime <= 0) {
            world.DestroyEntity(e);
        }
    });
}

void CombatSystem::MeleeAttack(ECSWorld& world, Entity attacker) {
    auto* combat = world.GetComponent<CombatComponent>(attacker);
    if (!combat || combat->CooldownTimer > 0) return;

    auto* tr = world.GetComponent<TransformComponent>(attacker);
    if (!tr) return;

    combat->IsAttacking = true;
    combat->AttackTimer = combat->AttackDuration;
    combat->CooldownTimer = combat->AttackCooldown;

    glm::vec2 attackerPos = {tr->X, tr->Y};

    // 对范围内敌方实体造成伤害
    // 攻击方向基于玩家朝向 (RotationZ 表示朝向角度)
    f32 attackAngle = tr->RotZ;
    glm::vec2 attackDir = {std::cos(attackAngle), std::sin(attackAngle)};

    world.ForEach<HealthComponent>([&](Entity target, HealthComponent& hp) {
        if (target == attacker) return;
        auto* ttr = world.GetComponent<TransformComponent>(target);
        if (!ttr) return;

        glm::vec2 targetPos = {ttr->X, ttr->Y};
        glm::vec2 diff = targetPos - attackerPos;
        f32 dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);

        if (dist > combat->AttackRange) return;

        // 检查是否在攻击方向的 120° 扇形内
        if (dist > 0.01f) {
            glm::vec2 dirToTarget = diff / dist;
            f32 dot = attackDir.x * dirToTarget.x + attackDir.y * dirToTarget.y;
            if (dot < -0.5f) return;  // cos(120°) = -0.5, 在背后就不打
        }

        DealDamage(world, target, combat->AttackDamage,
                   dist > 0.01f ? diff / dist : glm::vec2{0,0},
                   combat->KnockbackForce);
    });
}

void CombatSystem::DealDamage(ECSWorld& world, Entity target, f32 damage,
                               const glm::vec2& knockDir, f32 knockForce) {
    auto* hp = world.GetComponent<HealthComponent>(target);
    if (!hp) return;

    // 减去防御
    auto* combat = world.GetComponent<CombatComponent>(target);
    f32 actualDamage = damage;
    if (combat) actualDamage = std::max(1.0f, damage - combat->Defense);

    hp->Current -= actualDamage;
    if (hp->Current < 0) hp->Current = 0;

    // 击退
    if (knockForce > 0 && (knockDir.x != 0 || knockDir.y != 0)) {
        auto* tr = world.GetComponent<TransformComponent>(target);
        if (tr) {
            tr->X += knockDir.x * knockForce * 0.1f;
            tr->Y += knockDir.y * knockForce * 0.1f;
        }
    }
}

bool CombatSystem::IsDead(ECSWorld& world, Entity e) const {
    auto* hp = world.GetComponent<HealthComponent>(e);
    return hp && hp->Current <= 0;
}

void CombatSystem::SpawnLoot(ECSWorld& world, const glm::vec2& pos,
                              u32 itemID, u32 count) {
    Entity e = world.CreateEntity("Loot");
    auto& tr = world.AddComponent<TransformComponent>(e);
    tr.X = pos.x;
    tr.Y = pos.y;
    tr.ScaleX = tr.ScaleY = tr.ScaleZ = 0.4f;

    auto& loot = world.AddComponent<LootDropComponent>(e);
    loot.ItemID = itemID;
    loot.Count = count;
}

void CombatSystem::PickupLoot(ECSWorld& world, Entity player) {
    auto* ptr = world.GetComponent<TransformComponent>(player);
    if (!ptr) return;
    glm::vec2 playerPos = {ptr->X, ptr->Y};

    // 临时收集需要销毁的实体
    std::vector<Entity> toDestroy;

    world.ForEach<LootDropComponent>([&](Entity e, LootDropComponent& loot) {
        auto* ltr = world.GetComponent<TransformComponent>(e);
        if (!ltr) return;

        glm::vec2 lootPos = {ltr->X, ltr->Y};
        f32 dx = lootPos.x - playerPos.x;
        f32 dy = lootPos.y - playerPos.y;
        f32 dist = std::sqrt(dx * dx + dy * dy);

        if (dist <= loot.PickupRange) {
            // 尝试添加到背包
            auto* inv = world.GetComponent<InventoryComponent>(player);
            if (inv) {
                inv->AddItem(loot.ItemID, loot.Count);
            }
            toDestroy.push_back(e);
        }
    });

    for (auto e : toDestroy) {
        world.DestroyEntity(e);
    }
}

} // namespace Engine
