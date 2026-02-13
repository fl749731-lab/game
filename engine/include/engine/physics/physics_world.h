#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"
#include "engine/physics/collision.h"

#include <glm/glm.hpp>
#include <vector>
#include <functional>
#include <unordered_set>

namespace Engine {

// ── 碰撞体组件（支持多形状 + 碰撞层 + 复合碰撞体）────────

struct ColliderShape_Data {
    ColliderShape Shape = ColliderShape::Box;
    glm::vec3 Offset = {0, 0, 0};   // 相对于实体中心的偏移

    // Box
    AABB LocalBounds = {};

    // Sphere
    f32 SphereRadius = 0.5f;

    // Capsule
    f32 CapsuleRadius = 0.25f;
    f32 CapsuleHeight = 1.0f;   // 包含两端半球的总高度
};

struct ColliderComponent : public Component {
    // 主碰撞体形状
    ColliderShape Shape = ColliderShape::Box;
    AABB LocalBounds = {};          // Box 用
    f32  SphereRadius = 0.5f;       // Sphere 用
    f32  CapsuleRadius = 0.25f;     // Capsule 用
    f32  CapsuleHeight = 1.0f;      // Capsule 用（含半球总高）

    // 碰撞层/掩码
    u16 Layer = CollisionLayer::Default;   // 自己所在层
    u16 Mask  = CollisionLayer::All;       // 可以碰撞的层掩码

    // 标志
    bool IsTrigger = false;   // 只触发事件，不产生物理响应
    bool UseCCD    = false;   // 启用连续碰撞检测

    // 物理材质
    PhysicsMaterial Material;

    // 复合碰撞体（多个子碰撞体组合）
    std::vector<ColliderShape_Data> SubShapes;

    /// 获取世界空间 AABB（所有形状的包围盒并集）
    AABB GetWorldAABB(const TransformComponent& tr) const {
        glm::vec3 pos = {tr.X, tr.Y, tr.Z};
        glm::vec3 scale = {tr.ScaleX, tr.ScaleY, tr.ScaleZ};

        AABB result;

        if (SubShapes.empty()) {
            // 单碰撞体
            result = GetShapeAABB(Shape, LocalBounds, SphereRadius,
                                  CapsuleRadius, CapsuleHeight,
                                  pos, scale, {0,0,0});
        } else {
            // 复合碰撞体 — 合并所有子形状
            result.Min = glm::vec3( 1e30f);
            result.Max = glm::vec3(-1e30f);
            for (auto& sub : SubShapes) {
                AABB subAABB = GetShapeAABB(sub.Shape, sub.LocalBounds,
                                            sub.SphereRadius,
                                            sub.CapsuleRadius, sub.CapsuleHeight,
                                            pos, scale, sub.Offset);
                result.Merge(subAABB);
            }
        }

        return result;
    }

    /// 获取世界空间球体
    Sphere GetWorldSphere(const TransformComponent& tr) const {
        f32 maxScale = std::max({tr.ScaleX, tr.ScaleY, tr.ScaleZ});
        return {{tr.X, tr.Y, tr.Z}, SphereRadius * maxScale};
    }

    /// 获取世界空间胶囊体
    Capsule GetWorldCapsule(const TransformComponent& tr) const {
        f32 halfH = (CapsuleHeight - 2.0f * CapsuleRadius) * 0.5f * tr.ScaleY;
        f32 r = CapsuleRadius * std::max(tr.ScaleX, tr.ScaleZ);
        glm::vec3 center = {tr.X, tr.Y, tr.Z};
        return {center + glm::vec3(0, -halfH, 0),
                center + glm::vec3(0,  halfH, 0), r};
    }

private:
    static AABB GetShapeAABB(ColliderShape shape, const AABB& localBounds,
                              f32 sphereR, f32 capR, f32 capH,
                              const glm::vec3& pos, const glm::vec3& scale,
                              const glm::vec3& offset) {
        glm::vec3 worldPos = pos + offset * scale;
        AABB result;
        switch (shape) {
        case ColliderShape::Box:
            result.Min = localBounds.Min * scale + worldPos;
            result.Max = localBounds.Max * scale + worldPos;
            break;
        case ColliderShape::Sphere: {
            f32 r = sphereR * std::max({scale.x, scale.y, scale.z});
            result.Min = worldPos - glm::vec3(r);
            result.Max = worldPos + glm::vec3(r);
            break;
        }
        case ColliderShape::Capsule: {
            f32 halfH = (capH - 2.0f * capR) * 0.5f * scale.y;
            f32 r = capR * std::max(scale.x, scale.z);
            result.Min = worldPos - glm::vec3(r, halfH + r, r);
            result.Max = worldPos + glm::vec3(r, halfH + r, r);
            break;
        }
        }
        return result;
    }
};

// ── 刚体组件 ────────────────────────────────────────────────

struct RigidBodyComponent : public Component {
    // ── 线性运动 ─────────────────────────────────────────
    glm::vec3 Velocity     = {0, 0, 0};
    glm::vec3 Acceleration = {0, 0, 0};

    // ── 角运动 ───────────────────────────────────────────
    glm::vec3 AngularVelocity = {0, 0, 0};

    // ── 物理属性 ─────────────────────────────────────────
    f32 Mass           = 1.0f;
    f32 Restitution    = 0.3f;
    f32 Friction       = 0.5f;
    f32 LinearDamping  = 0.01f;
    f32 AngularDamping = 0.05f;

    // ── 标志 ─────────────────────────────────────────────
    bool IsStatic   = false;
    bool UseGravity = true;

    // ── 休眠系统 ─────────────────────────────────────────
    bool CanSleep       = true;   // 允许休眠
    bool IsSleeping     = false;  // 当前是否休眠
    f32  SleepTimer     = 0.0f;   // 低速持续时间
    f32  SleepThreshold = 0.05f;  // 速度低于此阈值开始计时
    f32  SleepDelay     = 1.0f;   // 低速持续多久后休眠（秒）

    glm::vec3 GravityOverride = {0, -9.81f, 0};

    /// 唤醒刚体
    void WakeUp() {
        IsSleeping = false;
        SleepTimer = 0.0f;
    }
};

// ── 角色控制器组件 ──────────────────────────────────────────

struct CharacterControllerComponent : public Component {
    // 形状参数
    f32 Height    = 1.8f;     // 总高度
    f32 Radius    = 0.3f;     // 胶囊半径

    // 运动参数
    f32 MoveSpeed     = 5.0f;     // 移动速度 (m/s)
    f32 JumpForce     = 8.0f;     // 跳跃力
    f32 Gravity       = -20.0f;   // 重力加速度
    f32 StepHeight    = 0.3f;     // 可攀爬台阶高度
    f32 SlopeLimit    = 45.0f;    // 最大可行走坡度（度）

    // 状态
    bool IsGrounded   = false;    // 是否在地面
    bool WantsJump    = false;    // 本帧是否请求跳跃
    glm::vec3 MoveDir = {0,0,0};  // 本帧移动方向（归一化）
    f32 VerticalSpeed = 0.0f;     // 垂直速度（重力累积）

    // 碰撞层
    u16 Layer = CollisionLayer::Player;
    u16 Mask  = CollisionLayer::All & ~CollisionLayer::Trigger;
};

// ── 碰撞回调 ────────────────────────────────────────────────

using CollisionCallback = std::function<void(Entity a, Entity b, const glm::vec3& normal)>;
using CollisionEventCallback = std::function<void(const CollisionEventData& e)>;

// ── 关节/约束类型 ────────────────────────────────────────────

enum class ConstraintType : u8 {
    Distance,
    Spring,
    Hinge,
    PointToPoint,
};

struct Constraint {
    ConstraintType Type = ConstraintType::Distance;
    Entity EntityA = INVALID_ENTITY;
    Entity EntityB = INVALID_ENTITY;

    glm::vec3 AnchorA = {0, 0, 0};
    glm::vec3 AnchorB = {0, 0, 0};

    f32 Distance    = 1.0f;
    f32 Stiffness   = 100.0f;
    f32 Damping     = 5.0f;

    glm::vec3 HingeAxis = {0, 1, 0};
    f32 MinAngle = -3.14159f;
    f32 MaxAngle =  3.14159f;

    bool Enabled = true;
};

// ── CCD 结果 ────────────────────────────────────────────────

struct CCDResult {
    bool Hit = false;
    f32  TOI = 1.0f;
    glm::vec3 HitPoint = {};
    glm::vec3 HitNormal = {};
    Entity HitEntity = INVALID_ENTITY;
};

// ── 物理世界 ────────────────────────────────────────────────

class PhysicsWorld {
public:
    /// 每帧更新物理（固定步长内调用）
    static void Step(ECSWorld& world, f32 dt);

    /// 固定步长累积器更新（替代直接调用 Step，保证稳定性）
    /// frameTime = 实际帧时间，内部按 fixedDt 分步
    static void Update(ECSWorld& world, f32 frameTime);

    /// 设置固定步长（默认 1/60）
    static void SetFixedTimestep(f32 dt);
    static f32  GetFixedTimestep();

    // ── 力 / 冲量 ───────────────────────────────────────
    static void AddForce(ECSWorld& world, Entity e, const glm::vec3& force);
    static void AddImpulse(ECSWorld& world, Entity e, const glm::vec3& impulse);
    static void AddTorque(ECSWorld& world, Entity e, const glm::vec3& torque);

    // ── 射线检测 ─────────────────────────────────────────
    static HitResult Raycast(ECSWorld& world, const Ray& ray,
                             Entity* outEntity = nullptr,
                             u16 layerMask = CollisionLayer::All);

    // ── 碰撞回调 ─────────────────────────────────────────
    static void SetCollisionCallback(CollisionCallback cb);
    static void SetCollisionEventCallback(CollisionEventCallback cb);
    static const std::vector<CollisionPair>& GetCollisionPairs();
    static const std::vector<CollisionEventData>& GetCollisionEvents();

    // ── 地面 ─────────────────────────────────────────────
    static void SetGroundPlane(f32 height);
    static f32  GetGroundPlane();

    // ── 约束 ─────────────────────────────────────────────
    static u32  AddConstraint(const Constraint& c);
    static void RemoveConstraint(u32 id);
    static Constraint* GetConstraint(u32 id);
    static void ClearConstraints();
    static u32  GetConstraintCount();

    // ── CCD ──────────────────────────────────────────────
    static CCDResult SweepTest(ECSWorld& world, Entity e,
                               const glm::vec3& displacement);

private:
    static void IntegrateForces(ECSWorld& world, f32 dt);
    static void UpdateSleep(ECSWorld& world, f32 dt);
    static void DetectCollisions(ECSWorld& world);
    static void UpdateCollisionEvents();
    static void ResolveCollisions(ECSWorld& world);
    static void ResolveGroundCollisions(ECSWorld& world);
    static void SolveConstraints(ECSWorld& world, f32 dt);
    static void PerformCCD(ECSWorld& world, f32 dt);
    static void UpdateCharacterControllers(ECSWorld& world, f32 dt);

    // 通用碰撞检测（自动根据形状分派）
    static bool TestColliders(const ColliderComponent& colA, const TransformComponent& trA,
                              const ColliderComponent& colB, const TransformComponent& trB,
                              glm::vec3& outNormal, f32& outPenetration);

    static std::vector<CollisionPair> s_Pairs;
    static CollisionCallback s_Callback;
    static CollisionEventCallback s_EventCallback;
    static f32 s_GroundHeight;
    static std::vector<Constraint> s_Constraints;
    static u32 s_ConstraintIdCounter;
    static constexpr i32 CONSTRAINT_ITERATIONS = 8;

    // 固定步长
    static f32 s_FixedTimestep;
    static f32 s_Accumulator;

    // 碰撞事件追踪 (OnEnter/OnStay/OnExit)
    struct PairKey {
        u32 a, b;
        bool operator==(const PairKey& o) const {
            return (a == o.a && b == o.b) || (a == o.b && b == o.a);
        }
    };
    struct PairHash {
        size_t operator()(const PairKey& k) const {
            u32 lo = std::min(k.a, k.b);
            u32 hi = std::max(k.a, k.b);
            return std::hash<u64>()((u64)lo << 32 | hi);
        }
    };
    static std::unordered_set<PairKey, PairHash> s_PreviousPairs;
    static std::unordered_set<PairKey, PairHash> s_CurrentPairs;
    static std::vector<CollisionEventData> s_CollisionEvents;
};

} // namespace Engine
