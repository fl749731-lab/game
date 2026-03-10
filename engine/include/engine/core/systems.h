#pragma once

// ── 引擎内置系统 ────────────────────────────────────────────
// 从 ecs.h 拆分出的独立 System 头文件。
// 每个 System 继承 System 基类，在 ECSWorld::Update() 中被调用。

#include "engine/core/ecs.h"
#include "engine/core/components.h"

namespace Engine {

// ── 运动系统 ────────────────────────────────────────────────
/// 根据速度更新位置（SoA 直接遍历）

class MovementSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override {
        // 并行更新：每个实体只写自己的 Transform，线程安全
        auto& pool = world.GetComponentArray<VelocityComponent>();
        u32 count = pool.Size();
        JobSystem::ParallelFor(0u, count, [&](u32 i) {
            VelocityComponent& vel = pool.Data(i);
            auto* tr = world.GetComponent<TransformComponent>(pool.GetEntity(i));
            if (!tr) return;
            tr->X += vel.VX * dt;
            tr->Y += vel.VY * dt;
            tr->Z += vel.VZ * dt;
        });
    }
    const char* GetName() const override { return "MovementSystem"; }
};

// ── 生命周期系统 ────────────────────────────────────────────
/// 每帧更新倒计时，到期则标记删除

class LifetimeSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override {
        m_ToDestroy.clear();
        auto& pool = world.GetComponentArray<LifetimeComponent>();
        u32 count = pool.Size();
        for (u32 i = 0; i < count; i++) {
            LifetimeComponent& lc = pool.Data(i);
            lc.TimeRemaining -= dt;
            if (lc.TimeRemaining <= 0)
                m_ToDestroy.push_back(pool.GetEntity(i));
        }
        for (auto e : m_ToDestroy) {
            world.DestroyEntity(e);
        }
    }
    const char* GetName() const override { return "LifetimeSystem"; }
private:
    std::vector<Entity> m_ToDestroy;
};

// ── Transform 层级系统 ──────────────────────────────────────
/// 递归计算世界矩阵

class TransformSystem : public System {
public:
    void Update(ECSWorld& world, f32 dt) override {
        (void)dt;
        // 只处理根实体（无父节点），递归向下计算
        for (auto e : world.GetEntities()) {
            auto* tr = world.GetComponent<TransformComponent>(e);
            if (tr && tr->Parent == INVALID_ENTITY) {
                UpdateWorldMatrix(world, e, glm::mat4(1.0f));
            }
        }
    }
    const char* GetName() const override { return "TransformSystem"; }

private:
    void UpdateWorldMatrix(ECSWorld& world, Entity e, const glm::mat4& parentWorld) {
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) return;

        tr->WorldMatrix = parentWorld * tr->GetLocalMatrix();
        tr->WorldMatrixDirty = false;

        for (Entity child : tr->Children) {
            UpdateWorldMatrix(world, child, tr->WorldMatrix);
        }
    }
};

} // namespace Engine
