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
    bool UseCCD = false;     // true = 启用连续碰撞检测（高速物体防穿透）

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

// ── 关节/约束类型 ────────────────────────────────────────────

enum class ConstraintType : u8 {
    Distance,     // 距离约束 — 两实体间保持固定距离
    Spring,       // 弹簧约束 — 两实体间弹簧连接
    Hinge,        // 铰链约束 — 绕一条轴旋转
    PointToPoint, // 点对点约束 — 球关节（任意方向旋转）
};

struct Constraint {
    ConstraintType Type = ConstraintType::Distance;
    Entity EntityA = INVALID_ENTITY;
    Entity EntityB = INVALID_ENTITY;

    // 锚点（各自局部空间）
    glm::vec3 AnchorA = {0, 0, 0};
    glm::vec3 AnchorB = {0, 0, 0};

    // Distance / Spring 参数
    f32 Distance    = 1.0f;      // 目标距离
    f32 Stiffness   = 100.0f;    // 弹簧刚度 (Spring)
    f32 Damping     = 5.0f;      // 弹簧阻尼 (Spring)

    // Hinge 参数
    glm::vec3 HingeAxis = {0, 1, 0};  // 铰链轴（世界空间）
    f32 MinAngle = -3.14159f;         // 角度限制（弧度）
    f32 MaxAngle =  3.14159f;

    bool Enabled = true;
};

// ── CCD 结果 ────────────────────────────────────────────────

struct CCDResult {
    bool Hit = false;
    f32  TOI = 1.0f;          // Time of Impact [0,1]
    glm::vec3 HitPoint = {};
    glm::vec3 HitNormal = {};
    Entity HitEntity = INVALID_ENTITY;
};

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

    // ── 关节/约束 API ───────────────────────────────────
    /// 添加约束
    static u32  AddConstraint(const Constraint& c);
    /// 移除约束
    static void RemoveConstraint(u32 id);
    /// 获取约束（可修改）
    static Constraint* GetConstraint(u32 id);
    /// 清除所有约束
    static void ClearConstraints();
    /// 获取约束数量
    static u32  GetConstraintCount();

    // ── CCD API ─────────────────────────────────────────
    /// 对单个实体进行连续碰撞检测
    static CCDResult SweepTest(ECSWorld& world, Entity e, 
                               const glm::vec3& displacement);

private:
    static void IntegrateForces(ECSWorld& world, f32 dt);
    static void DetectCollisions(ECSWorld& world);
    static void ResolveCollisions(ECSWorld& world);
    static void ResolveGroundCollisions(ECSWorld& world);
    static void SolveConstraints(ECSWorld& world, f32 dt);
    static void PerformCCD(ECSWorld& world, f32 dt);

    static std::vector<CollisionPair> s_Pairs;
    static CollisionCallback s_Callback;
    static f32 s_GroundHeight;
    static std::vector<Constraint> s_Constraints;
    static u32 s_ConstraintIdCounter;
    static constexpr i32 CONSTRAINT_ITERATIONS = 8; // 约束迭代次数
};

} // namespace Engine
