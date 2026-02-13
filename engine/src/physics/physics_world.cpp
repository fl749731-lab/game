#include "engine/physics/physics_world.h"
#include "engine/core/event.h"
#include "engine/core/log.h"

#include <algorithm>
#include <cmath>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

std::vector<CollisionPair> PhysicsWorld::s_Pairs;
CollisionCallback PhysicsWorld::s_Callback = nullptr;
f32 PhysicsWorld::s_GroundHeight = 0.0f;

// ── 主更新 ──────────────────────────────────────────────────

void PhysicsWorld::Step(ECSWorld& world, f32 dt) {
    IntegrateForces(world, dt);
    DetectCollisions(world);
    ResolveCollisions(world);
    ResolveGroundCollisions(world);
}

// ── 力积分（SoA 直接遍历）──────────────────────────────────

void PhysicsWorld::IntegrateForces(ECSWorld& world, f32 dt) {
    auto& rbPool = world.GetComponentArray<RigidBodyComponent>();
    u32 count = rbPool.Size();

    for (u32 i = 0; i < count; i++) {
        RigidBodyComponent& rb = rbPool.Data(i);
        if (rb.IsStatic) continue;

        Entity e = rbPool.GetEntity(i);
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) continue;

        // 重力
        if (rb.UseGravity) {
            rb.Velocity += rb.GravityOverride * dt;
        }

        // 加速度（含外部施加的力）
        rb.Velocity += rb.Acceleration * dt;

        // 阻尼（空气阻力 / 线性拖拽）
        rb.Velocity *= (1.0f - rb.LinearDamping * dt);

        // 半隐式 Euler: 先更新速度再更新位置（更稳定）
        tr->X += rb.Velocity.x * dt;
        tr->Y += rb.Velocity.y * dt;
        tr->Z += rb.Velocity.z * dt;

        // 角速度积分
        if (glm::length(rb.AngularVelocity) > 1e-6f) {
            tr->RotX += glm::degrees(rb.AngularVelocity.x) * dt;
            tr->RotY += glm::degrees(rb.AngularVelocity.y) * dt;
            tr->RotZ += glm::degrees(rb.AngularVelocity.z) * dt;
            // 角速度阻尼
            rb.AngularVelocity *= (1.0f - rb.AngularDamping * dt);
        }

        // 每帧清零加速度（力需要每帧重新施加）
        rb.Acceleration = {0, 0, 0};
    }
}

// ── 碰撞检测（SoA + SpatialHash）──────────────────────────

void PhysicsWorld::DetectCollisions(ECSWorld& world) {
    s_Pairs.clear();

    // 从 SoA 数组直接收集碰撞数据
    auto& colPool = world.GetComponentArray<ColliderComponent>();
    u32 colCount = colPool.Size();

    struct ColEntity {
        Entity e;
        AABB worldAABB;
        bool isTrigger;
    };
    std::vector<ColEntity> entities;
    entities.reserve(colCount);

    for (u32 i = 0; i < colCount; i++) {
        ColliderComponent& col = colPool.Data(i);
        Entity e = colPool.GetEntity(i);
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) continue;
        entities.push_back({e, col.GetWorldAABB(*tr), col.IsTrigger});
    }

    // 窄相检测 lambda
    auto narrowPhase = [&](size_t i, size_t j) {
        glm::vec3 normal;
        f32 penetration;
        if (Collision::TestAABB(entities[i].worldAABB, entities[j].worldAABB,
                                normal, penetration)) {
            CollisionPair pair;
            pair.EntityA = entities[i].e;
            pair.EntityB = entities[j].e;
            pair.Normal = normal;
            pair.Penetration = penetration;
            s_Pairs.push_back(pair);

            if (s_Callback) {
                s_Callback(pair.EntityA, pair.EntityB, pair.Normal);
            }

            CollisionEvent evt(pair.EntityA, pair.EntityB,
                               normal.x, normal.y, normal.z, penetration);
            EventBus::Dispatch(evt);
        }
    };

    if (entities.size() <= 32) {
        // 暴力检测快速路径
        for (size_t i = 0; i < entities.size(); i++) {
            for (size_t j = i + 1; j < entities.size(); j++) {
                narrowPhase(i, j);
            }
        }
    } else {
        // SpatialHash 宽相加速
        SpatialHash grid(4.0f);
        std::unordered_map<u32, size_t> entityIndex;
        for (size_t i = 0; i < entities.size(); i++) {
            grid.Insert(entities[i].e, entities[i].worldAABB);
            entityIndex[entities[i].e] = i;
        }
        auto potentialPairs = grid.GetPotentialPairs();
        for (auto& [a, b] : potentialPairs) {
            auto itA = entityIndex.find(a);
            auto itB = entityIndex.find(b);
            if (itA != entityIndex.end() && itB != entityIndex.end()) {
                narrowPhase(itA->second, itB->second);
            }
        }
    }
}

// ── 碰撞响应（正确的基于质量的冲量模型）──────────────────

void PhysicsWorld::ResolveCollisions(ECSWorld& world) {
    for (auto& pair : s_Pairs) {
        auto* colA = world.GetComponent<ColliderComponent>(pair.EntityA);
        auto* colB = world.GetComponent<ColliderComponent>(pair.EntityB);
        if (!colA || !colB) continue;
        if (colA->IsTrigger || colB->IsTrigger) continue;

        auto* rbA = world.GetComponent<RigidBodyComponent>(pair.EntityA);
        auto* rbB = world.GetComponent<RigidBodyComponent>(pair.EntityB);
        auto* trA = world.GetComponent<TransformComponent>(pair.EntityA);
        auto* trB = world.GetComponent<TransformComponent>(pair.EntityB);

        bool staticA = (!rbA || rbA->IsStatic);
        bool staticB = (!rbB || rbB->IsStatic);

        if (staticA && staticB) continue;

        // ── 位置分离（按质量比例分配）──────────────────
        f32 massA = staticA ? 0.0f : rbA->Mass;
        f32 massB = staticB ? 0.0f : rbB->Mass;
        f32 totalInvMass = (massA > 0 ? 1.0f/massA : 0.0f) 
                         + (massB > 0 ? 1.0f/massB : 0.0f);

        if (totalInvMass > 0) {
            f32 invMassA = massA > 0 ? 1.0f/massA : 0.0f;
            f32 invMassB = massB > 0 ? 1.0f/massB : 0.0f;
            // 分离量按逆质量比分配（重的物体移动少）
            glm::vec3 correction = pair.Normal * pair.Penetration / totalInvMass;

            if (trA && !staticA) {
                trA->X -= correction.x * invMassA;
                trA->Y -= correction.y * invMassA;
                trA->Z -= correction.z * invMassA;
            }
            if (trB && !staticB) {
                trB->X += correction.x * invMassB;
                trB->Y += correction.y * invMassB;
                trB->Z += correction.z * invMassB;
            }
        }

        // ── 冲量速度解算（基于动量守恒 + 恢复系数）────
        glm::vec3 velA = staticA ? glm::vec3(0) : rbA->Velocity;
        glm::vec3 velB = staticB ? glm::vec3(0) : rbB->Velocity;
        glm::vec3 relVel = velA - velB;  // A 相对 B 的速度
        f32 velAlongNormal = glm::dot(relVel, pair.Normal);

        // 正在分离则不处理
        if (velAlongNormal > 0) continue;

        // 恢复系数取二者最小值
        f32 restitution = 0.3f;
        if (rbA && rbB)      restitution = std::min(rbA->Restitution, rbB->Restitution);
        else if (rbA)        restitution = rbA->Restitution;
        else if (rbB)        restitution = rbB->Restitution;

        // 冲量标量: j = -(1 + e) * Vrel·n / (1/mA + 1/mB)
        f32 j = -(1.0f + restitution) * velAlongNormal / totalInvMass;

        // 法向冲量
        glm::vec3 impulse = pair.Normal * j;
        if (rbA && !rbA->IsStatic) rbA->Velocity += impulse / rbA->Mass;
        if (rbB && !rbB->IsStatic) rbB->Velocity -= impulse / rbB->Mass;

        // ── 摩擦冲量（切向）──────────────────────────
        // 重新计算相对速度（冲量后）
        velA = staticA ? glm::vec3(0) : rbA->Velocity;
        velB = staticB ? glm::vec3(0) : rbB->Velocity;
        relVel = velA - velB;
        glm::vec3 tangent = relVel - pair.Normal * glm::dot(relVel, pair.Normal);
        f32 tangentLen = glm::length(tangent);
        if (tangentLen > 1e-6f) {
            tangent /= tangentLen;
            f32 jt = -glm::dot(relVel, tangent) / totalInvMass;
            // 库仑摩擦：摩擦冲量不超过 μ * 法向冲量
            f32 friction = 0.5f;
            if (rbA) friction = rbA->Friction;
            if (rbB) friction = std::min(friction, rbB->Friction);
            f32 maxFriction = friction * std::abs(j);
            jt = glm::clamp(jt, -maxFriction, maxFriction);

            glm::vec3 frictionImpulse = tangent * jt;
            if (rbA && !rbA->IsStatic) rbA->Velocity += frictionImpulse / rbA->Mass;
            if (rbB && !rbB->IsStatic) rbB->Velocity -= frictionImpulse / rbB->Mass;
        }
    }
}

// ── 地面碰撞（SoA 直接遍历）────────────────────────────

void PhysicsWorld::ResolveGroundCollisions(ECSWorld& world) {
    auto& rbPool = world.GetComponentArray<RigidBodyComponent>();
    u32 count = rbPool.Size();

    for (u32 i = 0; i < count; i++) {
        RigidBodyComponent& rb = rbPool.Data(i);
        if (rb.IsStatic) continue;

        Entity e = rbPool.GetEntity(i);
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) continue;

        auto* col = world.GetComponent<ColliderComponent>(e);
        f32 bottom = tr->Y;
        if (col) {
            bottom = tr->Y + col->LocalBounds.Min.y * tr->ScaleY;
        }

        if (bottom < s_GroundHeight) {
            f32 penetration = s_GroundHeight - bottom;
            tr->Y += penetration;
            if (rb.Velocity.y < 0) {
                rb.Velocity.y = -rb.Velocity.y * rb.Restitution;
                // 静止阈值
                if (std::abs(rb.Velocity.y) < 0.1f) {
                    rb.Velocity.y = 0;
                }
            }
            // 地面摩擦
            f32 frictionDecay = 1.0f - rb.Friction * 0.1f;
            rb.Velocity.x *= frictionDecay;
            rb.Velocity.z *= frictionDecay;
        }
    }
}

// ── 射线检测（SoA 直接遍历）────────────────────────────

HitResult PhysicsWorld::Raycast(ECSWorld& world, const Ray& ray, Entity* outEntity) {
    HitResult closest;
    closest.Distance = 1e30f;

    auto& colPool = world.GetComponentArray<ColliderComponent>();
    u32 count = colPool.Size();

    for (u32 i = 0; i < count; i++) {
        ColliderComponent& col = colPool.Data(i);
        Entity e = colPool.GetEntity(i);
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) continue;

        AABB worldAABB = col.GetWorldAABB(*tr);
        auto hit = Collision::RaycastAABB(ray, worldAABB);
        if (hit.Hit && hit.Distance < closest.Distance) {
            closest = hit;
            if (outEntity) *outEntity = e;
        }
    }

    if (closest.Distance >= 1e30f) closest.Hit = false;
    return closest;
}

// ── 设置/获取 ───────────────────────────────────────────────

void PhysicsWorld::SetCollisionCallback(CollisionCallback cb) { s_Callback = cb; }
const std::vector<CollisionPair>& PhysicsWorld::GetCollisionPairs() { return s_Pairs; }
void PhysicsWorld::SetGroundPlane(f32 height) { s_GroundHeight = height; }
f32  PhysicsWorld::GetGroundPlane() { return s_GroundHeight; }

// ── 力 / 冲量 ───────────────────────────────────────────────

void PhysicsWorld::AddForce(ECSWorld& world, Entity e, const glm::vec3& force) {
    auto* rb = world.GetComponent<RigidBodyComponent>(e);
    if (!rb || rb->IsStatic || rb->Mass <= 0) return;
    rb->Acceleration += force / rb->Mass;  // F = ma → a += F/m
}

void PhysicsWorld::AddImpulse(ECSWorld& world, Entity e, const glm::vec3& impulse) {
    auto* rb = world.GetComponent<RigidBodyComponent>(e);
    if (!rb || rb->IsStatic || rb->Mass <= 0) return;
    rb->Velocity += impulse / rb->Mass;  // Δv = J/m
}

void PhysicsWorld::AddTorque(ECSWorld& world, Entity e, const glm::vec3& torque) {
    auto* rb = world.GetComponent<RigidBodyComponent>(e);
    if (!rb || rb->IsStatic || rb->Mass <= 0) return;
    // 简化: 假设 I ≈ mass (均匀球体近似)
    rb->AngularVelocity += torque / rb->Mass;
}

} // namespace Engine
