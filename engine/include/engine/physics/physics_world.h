#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"
#include "engine/physics/collision.h"

#include <glm/glm.hpp>
#include <vector>
#include <functional>

namespace Engine {

// ── 碰撞体组件 ──────────────────────────────────────────────

struct ColliderComponent : public Component {
    AABB LocalBounds = {};   // 局部空间 AABB
    bool IsTrigger = false;  // true = 只触发事件，不产生物理响应

    /// 获取世界空间 AABB（需要 TransformComponent）
    AABB GetWorldAABB(const TransformComponent& tr) const {
        AABB world;
        world.Min = LocalBounds.Min * glm::vec3(tr.ScaleX, tr.ScaleY, tr.ScaleZ)
                   + glm::vec3(tr.X, tr.Y, tr.Z);
        world.Max = LocalBounds.Max * glm::vec3(tr.ScaleX, tr.ScaleY, tr.ScaleZ)
                   + glm::vec3(tr.X, tr.Y, tr.Z);
        return world;
    }
};

// ── 刚体组件 ────────────────────────────────────────────────

struct RigidBodyComponent : public Component {
    // ── 线性运动 ─────────────────────────────────────────
    glm::vec3 Velocity     = {0, 0, 0};
    glm::vec3 Acceleration = {0, 0, 0};

    // ── 角运动 ───────────────────────────────────────────
    glm::vec3 AngularVelocity = {0, 0, 0};  // rad/s

    // ── 物理属性 ─────────────────────────────────────────
    f32 Mass        = 1.0f;
    f32 Restitution = 0.3f;   // 弹性系数 0~1
    f32 Friction    = 0.5f;   // 摩擦系数
    f32 LinearDamping  = 0.01f;  // 线性阻尼（空气阻力）
    f32 AngularDamping = 0.05f;  // 角阻尼

    // ── 标志 ─────────────────────────────────────────────
    bool IsStatic   = false;  // 静态物体不受力
    bool UseGravity = true;

    glm::vec3 GravityOverride = {0, -9.81f, 0};
};

// ── 碰撞回调 ────────────────────────────────────────────────

using CollisionCallback = std::function<void(Entity a, Entity b, const glm::vec3& normal)>;

// ── 物理世界 ────────────────────────────────────────────────

class PhysicsWorld {
public:
    /// 每帧更新物理（固定步长内调用）
    static void Step(ECSWorld& world, f32 dt);

    /// 施加持续力（通过加速度累积，下帧生效，F = m*a）
    static void AddForce(ECSWorld& world, Entity e, const glm::vec3& force);

    /// 施加瞬间冲量（直接改变速度，用于爆炸/跳跃等）
    static void AddImpulse(ECSWorld& world, Entity e, const glm::vec3& impulse);

    /// 施加力矩（改变角速度）
    static void AddTorque(ECSWorld& world, Entity e, const glm::vec3& torque);

    /// 射线检测（返回第一个命中的实体碰撞信息）
    static HitResult Raycast(ECSWorld& world, const Ray& ray,
                             Entity* outEntity = nullptr);

    /// 设置碰撞回调
    static void SetCollisionCallback(CollisionCallback cb);

    /// 获取本帧碰撞对
    static const std::vector<CollisionPair>& GetCollisionPairs();

    /// 地面高度（简单平面碰撞）
    static void SetGroundPlane(f32 height);
    static f32  GetGroundPlane();

private:
    static void IntegrateForces(ECSWorld& world, f32 dt);
    static void DetectCollisions(ECSWorld& world);
    static void ResolveCollisions(ECSWorld& world);
    static void ResolveGroundCollisions(ECSWorld& world);

    static std::vector<CollisionPair> s_Pairs;
    static CollisionCallback s_Callback;
    static f32 s_GroundHeight;
};

} // namespace Engine
