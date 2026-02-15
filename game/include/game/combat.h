#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"

#include <glm/glm.hpp>
#include <string>

namespace Engine {

// ── 战斗组件 ──────────────────────────────────────────────

struct CombatComponent : public Component {
    f32 AttackDamage   = 10.0f;
    f32 AttackRange    = 1.2f;     // 近战范围 (世界单位)
    f32 AttackCooldown = 0.5f;     // 攻击间隔 (秒)
    f32 CooldownTimer  = 0.0f;     // 当前冷却
    f32 KnockbackForce = 3.0f;     // 击退力度
    f32 Defense        = 0.0f;     // 减伤

    bool IsAttacking   = false;
    f32  AttackTimer   = 0.0f;     // 攻击动画计时
    f32  AttackDuration = 0.15f;   // 攻击动作持续时间
};

// ── 远程武器组件 (可选, 后续扩展) ──────────────────────────

struct RangedWeaponComponent : public Component {
    f32 ProjectileSpeed = 15.0f;
    u32 AmmoCount = 0;
    u32 MaxAmmo   = 30;
    f32 FireRate  = 0.3f;
    f32 FireTimer = 0.0f;
};

// ── 投射物组件 ────────────────────────────────────────────

struct ProjectileComponent : public Component {
    f32 Damage = 5.0f;
    f32 Speed  = 15.0f;
    f32 Lifetime = 3.0f;
    Entity Owner = INVALID_ENTITY;  // 发射者
    glm::vec2 Direction = {0, 0};
};

// ── 掉落物组件 ────────────────────────────────────────────

struct LootDropComponent : public Component {
    u32 ItemID = 0;
    u32 Count  = 1;
    f32 PickupRange = 1.0f;
    f32 Lifetime = 30.0f;  // 30秒后消失
};

// ── 战斗系统 ──────────────────────────────────────────────

class CombatSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override;
    const char* GetName() const override { return "CombatSystem"; }

    /// 发起攻击 (近战): 对 attacker 前方范围内的敌人造成伤害
    void MeleeAttack(ECSWorld& world, Entity attacker);

    /// 造成伤害 (通用)
    void DealDamage(ECSWorld& world, Entity target, f32 damage,
                    const glm::vec2& knockDir = {0,0}, f32 knockForce = 0);

    /// 检查实体是否死亡
    bool IsDead(ECSWorld& world, Entity e) const;

    /// 生成掉落物
    void SpawnLoot(ECSWorld& world, const glm::vec2& pos,
                   u32 itemID, u32 count = 1);

    /// 拾取范围内掉落物
    void PickupLoot(ECSWorld& world, Entity player);
};

} // namespace Engine
