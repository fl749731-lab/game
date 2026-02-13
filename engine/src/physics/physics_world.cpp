#include "engine/physics/physics_world.h"
#include "engine/core/event.h"
#include "engine/core/log.h"

#include <algorithm>

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

// ── 力积分 ──────────────────────────────────────────────────

void PhysicsWorld::IntegrateForces(ECSWorld& world, f32 dt) {
    world.ForEach<RigidBodyComponent>([&](Entity e, RigidBodyComponent& rb) {
        if (rb.IsStatic) return;

        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) return;

        // 重力
        if (rb.UseGravity) {
            rb.Velocity += rb.GravityOverride * dt;
        }

        // 加速度（含外部施加的力）
        rb.Velocity += rb.Acceleration * dt;

        // 半隐式 Euler: 先更新速度再更新位置（更稳定）
        tr->X += rb.Velocity.x * dt;
        tr->Y += rb.Velocity.y * dt;
        tr->Z += rb.Velocity.z * dt;

        // 每帧清零加速度（力需要每帧重新施加）
        rb.Acceleration = {0, 0, 0};
    });
}

// ── 碰撞检测 ────────────────────────────────────────────────

void PhysicsWorld::DetectCollisions(ECSWorld& world) {
    s_Pairs.clear();

    // 收集所有有碰撞体的实体
    struct ColEntity {
        Entity e;
        AABB worldAABB;
        bool isTrigger;
    };
    std::vector<ColEntity> entities;

    world.ForEach<ColliderComponent>([&](Entity e, ColliderComponent& col) {
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) return;
        entities.push_back({e, col.GetWorldAABB(*tr), col.IsTrigger});
    });

    // O(n²) 暴力检测（小规模足够）
    for (size_t i = 0; i < entities.size(); i++) {
        for (size_t j = i + 1; j < entities.size(); j++) {
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

                // 触发回调（旧接口兼容）
                if (s_Callback) {
                    s_Callback(pair.EntityA, pair.EntityB, pair.Normal);
                }

                // 通过事件总线发布碰撞事件
                CollisionEvent evt(pair.EntityA, pair.EntityB,
                                   normal.x, normal.y, normal.z, penetration);
                EventBus::Dispatch(evt);
            }
        }
    }
}

// ── 碰撞响应 ────────────────────────────────────────────────

void PhysicsWorld::ResolveCollisions(ECSWorld& world) {
    for (auto& pair : s_Pairs) {
        auto* colA = world.GetComponent<ColliderComponent>(pair.EntityA);
        auto* colB = world.GetComponent<ColliderComponent>(pair.EntityB);
        if (!colA || !colB) continue;
        if (colA->IsTrigger || colB->IsTrigger) continue; // Trigger 不碰撞

        auto* rbA = world.GetComponent<RigidBodyComponent>(pair.EntityA);
        auto* rbB = world.GetComponent<RigidBodyComponent>(pair.EntityB);
        auto* trA = world.GetComponent<TransformComponent>(pair.EntityA);
        auto* trB = world.GetComponent<TransformComponent>(pair.EntityB);

        bool staticA = (!rbA || rbA->IsStatic);
        bool staticB = (!rbB || rbB->IsStatic);

        if (staticA && staticB) continue; // 两个都是静态

        // 分离
        if (staticA && trB) {
            trB->X += pair.Normal.x * pair.Penetration;
            trB->Y += pair.Normal.y * pair.Penetration;
            trB->Z += pair.Normal.z * pair.Penetration;
        } else if (staticB && trA) {
            trA->X -= pair.Normal.x * pair.Penetration;
            trA->Y -= pair.Normal.y * pair.Penetration;
            trA->Z -= pair.Normal.z * pair.Penetration;
        } else if (trA && trB) {
            f32 half = pair.Penetration * 0.5f;
            trA->X -= pair.Normal.x * half;
            trA->Y -= pair.Normal.y * half;
            trA->Z -= pair.Normal.z * half;
            trB->X += pair.Normal.x * half;
            trB->Y += pair.Normal.y * half;
            trB->Z += pair.Normal.z * half;
        }

        // 速度反弹
        f32 restitution = 0.3f;
        if (rbA && !rbA->IsStatic) {
            restitution = rbA->Restitution;
            f32 vn = glm::dot(rbA->Velocity, -pair.Normal);
            if (vn > 0) {
                rbA->Velocity += -pair.Normal * vn * (1.0f + restitution);
            }
        }
        if (rbB && !rbB->IsStatic) {
            restitution = rbB->Restitution;
            f32 vn = glm::dot(rbB->Velocity, pair.Normal);
            if (vn > 0) {
                rbB->Velocity += pair.Normal * vn * (1.0f + restitution);
            }
        }
    }
}

// ── 地面碰撞 ────────────────────────────────────────────────

void PhysicsWorld::ResolveGroundCollisions(ECSWorld& world) {
    world.ForEach<RigidBodyComponent>([&](Entity e, RigidBodyComponent& rb) {
        if (rb.IsStatic) return;
        auto* tr = world.GetComponent<TransformComponent>(e);
        auto* col = world.GetComponent<ColliderComponent>(e);
        if (!tr) return;

        f32 groundY = s_GroundHeight;
        f32 bottom = tr->Y;
        if (col) {
            bottom = tr->Y + col->LocalBounds.Min.y * tr->ScaleY;
        }

        if (bottom < groundY) {
            f32 penetration = groundY - bottom;
            tr->Y += penetration;
            if (rb.Velocity.y < 0) {
                rb.Velocity.y = -rb.Velocity.y * rb.Restitution;
                // 静止阈值
                if (fabsf(rb.Velocity.y) < 0.1f) {
                    rb.Velocity.y = 0;
                }
            }
            // 摩擦
            rb.Velocity.x *= (1.0f - rb.Friction * 0.1f);
            rb.Velocity.z *= (1.0f - rb.Friction * 0.1f);
        }
    });
}

// ── 射线检测 ────────────────────────────────────────────────

HitResult PhysicsWorld::Raycast(ECSWorld& world, const Ray& ray, Entity* outEntity) {
    HitResult closest;
    closest.Distance = 1e30f;

    world.ForEach<ColliderComponent>([&](Entity e, ColliderComponent& col) {
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) return;

        AABB worldAABB = col.GetWorldAABB(*tr);
        auto hit = Collision::RaycastAABB(ray, worldAABB);
        if (hit.Hit && hit.Distance < closest.Distance) {
            closest = hit;
            if (outEntity) *outEntity = e;
        }
    });

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

} // namespace Engine
