#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"
#include "engine/physics/collision.h"
#include "engine/physics/obb.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <mutex>

namespace Engine {

// ── 碰撞体组件（支持多形状 + 碰撞层 + 复合碰撞体）────────

struct ColliderShape_Data {
    ColliderShape Shape = ColliderShape::Box;
    glm::vec3 Offset = {0, 0, 0};

    // Box
    AABB LocalBounds = {};

    // Sphere
    f32 SphereRadius = 0.5f;

    // Capsule
    f32 CapsuleRadius = 0.25f;
    f32 CapsuleHeight = 1.0f;
};

struct ColliderComponent : public Component {
    // 主碰撞体形状
    ColliderShape Shape = ColliderShape::Box;
    AABB LocalBounds = {};
    f32  SphereRadius = 0.5f;
    f32  CapsuleRadius = 0.25f;
    f32  CapsuleHeight = 1.0f;

    // 碰撞层/掩码
    u16 Layer = CollisionLayer::Default;
    u16 Mask  = CollisionLayer::All;

    // 标志
    bool IsTrigger = false;
    bool UseCCD    = false;

    // 物理材质
    PhysicsMaterial Material;

    // 复合碰撞体
    std::vector<ColliderShape_Data> SubShapes;

    /// 获取世界空间 AABB
    AABB GetWorldAABB(const TransformComponent& tr) const {
        glm::vec3 pos = {tr.X, tr.Y, tr.Z};
        glm::vec3 scale = {tr.ScaleX, tr.ScaleY, tr.ScaleZ};

        AABB result;

        if (SubShapes.empty()) {
            result = GetShapeAABB(Shape, LocalBounds, SphereRadius,
                                  CapsuleRadius, CapsuleHeight,
                                  pos, scale, {0,0,0});
        } else {
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

    Sphere GetWorldSphere(const TransformComponent& tr) const {
        f32 maxScale = std::max({tr.ScaleX, tr.ScaleY, tr.ScaleZ});
        return {{tr.X, tr.Y, tr.Z}, SphereRadius * maxScale};
    }

    Capsule GetWorldCapsule(const TransformComponent& tr) const {
        f32 halfH = (CapsuleHeight - 2.0f * CapsuleRadius) * 0.5f * tr.ScaleY;
        f32 r = CapsuleRadius * std::max(tr.ScaleX, tr.ScaleZ);
        glm::vec3 center = {tr.X, tr.Y, tr.Z};
        return {center + glm::vec3(0, -halfH, 0),
                center + glm::vec3(0,  halfH, 0), r};
    }

    /// 获取世界空间 OBB
    OBB GetWorldOBB(const TransformComponent& tr) const {
        glm::vec3 pos = {tr.X, tr.Y, tr.Z};
        glm::vec3 scale = {tr.ScaleX, tr.ScaleY, tr.ScaleZ};
        glm::vec3 euler = glm::radians(glm::vec3(tr.RotX, tr.RotY, tr.RotZ));
        glm::quat rotation(euler);
        return OBB::FromTransform(pos, scale, rotation);
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
    glm::vec3 Velocity     = {0, 0, 0};
    glm::vec3 Acceleration = {0, 0, 0};
    glm::vec3 AngularVelocity = {0, 0, 0};

    f32 Mass           = 1.0f;
    f32 Restitution    = 0.3f;
    f32 Friction       = 0.5f;
    f32 LinearDamping  = 0.01f;
    f32 AngularDamping = 0.05f;

    bool IsStatic   = false;
    bool UseGravity = true;

    // 休眠系统
    bool CanSleep       = true;
    bool IsSleeping     = false;
    f32  SleepTimer     = 0.0f;
    f32  SleepThreshold = 0.05f;
    f32  SleepDelay     = 1.0f;

    glm::vec3 GravityOverride = {0, -9.81f, 0};

    void WakeUp() {
        IsSleeping = false;
        SleepTimer = 0.0f;
    }

    /// 计算逆质量（静态=0，安全除法）
    f32 InvMass() const {
        if (IsStatic || Mass <= 1e-6f) return 0.0f;
        return 1.0f / Mass;
    }
};

// ── 角色控制器 ──────────────────────────────────────────────

struct CharacterControllerComponent : public Component {
    f32 Height    = 1.8f;
    f32 Radius    = 0.3f;

    f32 MoveSpeed     = 5.0f;
    f32 JumpForce     = 8.0f;
    f32 Gravity       = -20.0f;
    f32 StepHeight    = 0.3f;
    f32 SlopeLimit    = 45.0f;

    bool IsGrounded   = false;
    bool WantsJump    = false;
    glm::vec3 MoveDir = {0,0,0};
    f32 VerticalSpeed = 0.0f;

    u16 Layer = CollisionLayer::Player;
    u16 Mask  = CollisionLayer::All & ~CollisionLayer::Trigger;
};

// ── 碰撞回调 ────────────────────────────────────────────────

using CollisionCallback = std::function<void(Entity a, Entity b, const glm::vec3& normal)>;
using CollisionEventCallback = std::function<void(const CollisionEventData& e)>;

// ── 约束句柄（Generation-Based，安全删除）──────────────────

struct ConstraintHandle {
    u32 Index = 0;
    u32 Generation = 0;
    bool IsValid() const { return Generation > 0; }
    bool operator==(const ConstraintHandle& o) const {
        return Index == o.Index && Generation == o.Generation;
    }
};

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

    // 内部管理
    u32 Generation_ = 0;  // 用于验证句柄有效性
    bool Active_    = false;
};

// ── CCD 结果 ────────────────────────────────────────────────

struct CCDResult {
    bool Hit = false;
    f32  TOI = 1.0f;
    glm::vec3 HitPoint = {};
    glm::vec3 HitNormal = {};
    Entity HitEntity = INVALID_ENTITY;
};

// ── 物理世界配置 ────────────────────────────────────────────

struct PhysicsConfig {
    f32 FixedTimestep    = 1.0f / 60.0f;
    f32 MaxAccumulator   = 0.25f;       // 防止死循环
    i32 ConstraintIters  = 8;           // 约束迭代次数
    i32 VelocityIters    = 4;           // 速度迭代次数
    f32 MaxVelocity      = 100.0f;      // 速度上限 (m/s)
    f32 MaxAngularVel    = 50.0f;       // 角速度上限 (rad/s)
    f32 PenetrationSlop  = 0.01f;       // 穿透容差
    f32 BaumgarteBias    = 0.2f;        // Baumgarte 偏差
    f32 MaxMassRatio     = 100.0f;      // 质量比上限
    f32 SleepLinear      = 0.05f;       // 休眠线性阈值
    f32 SleepAngular     = 0.05f;       // 休眠角速度阈值
    f32 SleepDelay       = 1.0f;        // 休眠延迟 (s)
};

// ── 物理世界 ────────────────────────────────────────────────

class PhysicsWorld {
public:
    /// 固定步长更新（推荐入口）
    static void Update(ECSWorld& world, f32 frameTime);

    /// 单步更新
    static void Step(ECSWorld& world, f32 dt);

    /// 配置
    static void SetConfig(const PhysicsConfig& cfg);
    static const PhysicsConfig& GetConfig();

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

    // ── 约束（Generation-Based 安全句柄）─────────────────
    static ConstraintHandle AddConstraint(const Constraint& c);
    static void RemoveConstraint(ConstraintHandle handle);
    static Constraint* GetConstraint(ConstraintHandle handle);
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

    // 通用碰撞检测（支持复合碰撞体的窄相）
    static bool TestColliders(const ColliderComponent& colA, const TransformComponent& trA,
                              const ColliderComponent& colB, const TransformComponent& trB,
                              glm::vec3& outNormal, f32& outPenetration);

    // 单形状碰撞（内部用）
    static bool TestSingleShape(ColliderShape shapeA, const ColliderComponent& colA,
                                const TransformComponent& trA, const glm::vec3& offsetA,
                                ColliderShape shapeB, const ColliderComponent& colB,
                                const TransformComponent& trB, const glm::vec3& offsetB,
                                glm::vec3& outNormal, f32& outPenetration);

    // 速度钳制
    static void ClampVelocities(RigidBodyComponent& rb);

    static std::vector<CollisionPair> s_Pairs;
    static CollisionCallback s_Callback;
    static CollisionEventCallback s_EventCallback;
    static f32 s_GroundHeight;

    // 约束池（slot 复用 + generation）
    static std::vector<Constraint> s_Constraints;
    static std::vector<u32> s_FreeSlots;   // 空闲槽位

    // 固定步长
    static f32 s_Accumulator;

    // 配置
    static PhysicsConfig s_Config;

    // 碰撞事件追踪
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

    // 线程安全
    static std::mutex s_Mutex;
};

} // namespace Engine
