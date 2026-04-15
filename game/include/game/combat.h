#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"

#include <glm/glm.hpp>
#include <string>

namespace Engine {

// ── 战斗组件 ──────────────────────────────────────────────

struct CombatComponent : public Component {
    f32 m_attackDamage   = 10.0f;
    f32 m_attackRange    = 1.2f;     // 近战范围 (世界单位)
    f32 m_attackCooldown = 0.5f;     // 攻击间隔 (秒)
    f32 m_cooldownTimer  = 0.0f;      // 当前冷却
    f32 m_knockbackForce = 3.0f;     // 击退力度
    f32 m_defense        = 0.0f;     // 减伤

    bool m_isAttacking   = false;
    f32  m_attackTimer   = 0.0f;      // 攻击动画计时
    f32  m_attackDuration = 0.15f;   // 攻击动作持续时间
};

// ── 远程武器组件 (可选, 后续扩展) ──────────────────────────

struct RangedWeaponComponent : public Component {
    f32 m_projectileSpeed = 15.0f;
    u32 m_ammoCount = 0;
    u32 m_maxAmmo   = 30;
    f32 m_fireRate  = 0.3f;
    f32 m_fireTimer = 0.0f;
};

// ── 投射物组件 ────────────────────────────────────────────

struct ProjectileComponent : public Component {
    f32 m_damage = 5.0f;
    f32 m_speed  = 15.0f;
    f32 m_lifetime = 3.0f;
    Entity m_owner = INVALID_ENTITY;  // 发射者
    glm::vec2 m_direction = {0, 0};
};

// ── 掉落物组件 ────────────────────────────────────────────

struct LootDropComponent : public Component {
    u32 m_itemID = 0;
    u32 m_count  = 1;
    f32 m_pickupRange = 1.0f;
    f32 m_lifetime = 30.0f;  // 30秒后消失
};

// ── 战斗系统 ──────────────────────────────────────────────

class CombatSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override;
    [[nodiscard]] const char* GetName() const override { return "CombatSystem"; }

    /// 发起攻击 (近战): 对 attacker 前方范围内的敌人造成伤害
    void MeleeAttack(ECSWorld& world, Entity attacker);

    /// 造成伤害 (通用)
    void DealDamage(ECSWorld& world, Entity target, f32 damage,
                    const glm::vec2& knockDir = {0,0}, f32 knockForce = 0);

    /// 检查实体是否死亡
    [[nodiscard]] bool IsDead(ECSWorld& world, Entity e) const;

    /// 生成掉落物
    void SpawnLoot(ECSWorld& world, const glm::vec2& pos,
                   u32 itemID, u32 count = 1);

    /// 拾取范围内掉落物
    void PickupLoot(ECSWorld& world, Entity player);

private:
    // cos(120°) = -0.5，用于扇形攻击范围判定
    static constexpr f32 k_cosAttackAngle = -0.5f;
    // 最小有效距离，防止除零
    static constexpr f32 k_minAttackDist = 0.01f;
    // 击退位移系数
    static constexpr f32 k_knockbackMult = 0.1f;
    // 最小伤害值
    static constexpr f32 k_minDamage = 1.0f;
};

} // namespace Engine
