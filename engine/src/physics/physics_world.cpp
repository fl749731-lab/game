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
std::vector<Constraint> PhysicsWorld::s_Constraints;
u32 PhysicsWorld::s_ConstraintIdCounter = 0;

// ── 主更新 ──────────────────────────────────────────────────

void PhysicsWorld::Step(ECSWorld& world, f32 dt) {
    IntegrateForces(world, dt);
    PerformCCD(world, dt);
    DetectCollisions(world);
    ResolveCollisions(world);
    SolveConstraints(world, dt);
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

        // 线性阻尼（空气阻力）
        rb.Velocity *= (1.0f - rb.LinearDamping * dt);

        // 半隐式 Euler: 先更新速度再更新位置
        tr->X += rb.Velocity.x * dt;
        tr->Y += rb.Velocity.y * dt;
        tr->Z += rb.Velocity.z * dt;

        // 角速度积分
        if (glm::length(rb.AngularVelocity) > 1e-6f) {
            tr->RotX += glm::degrees(rb.AngularVelocity.x) * dt;
            tr->RotY += glm::degrees(rb.AngularVelocity.y) * dt;
            tr->RotZ += glm::degrees(rb.AngularVelocity.z) * dt;
            rb.AngularVelocity *= (1.0f - rb.AngularDamping * dt);
        }

        // 每帧清零加速度
        rb.Acceleration = {0, 0, 0};
    }
}

// ── CCD 连续碰撞检测 ────────────────────────────────────────

CCDResult PhysicsWorld::SweepTest(ECSWorld& world, Entity e,
                                   const glm::vec3& displacement) {
    CCDResult result;
    auto* col = world.GetComponent<ColliderComponent>(e);
    auto* tr = world.GetComponent<TransformComponent>(e);
    if (!col || !tr) return result;

    AABB startAABB = col->GetWorldAABB(*tr);
    f32 dispLen = glm::length(displacement);
    if (dispLen < 1e-6f) return result;

    glm::vec3 dir = displacement / dispLen;

    // 构建运动扫掠 AABB（起点 + 终点的包围盒并集）
    AABB endAABB;
    endAABB.Min = startAABB.Min + displacement;
    endAABB.Max = startAABB.Max + displacement;

    AABB sweepAABB;
    sweepAABB.Min = glm::min(startAABB.Min, endAABB.Min);
    sweepAABB.Max = glm::max(startAABB.Max, endAABB.Max);

    // 对所有碰撞体进行扫掠检测
    auto& colPool = world.GetComponentArray<ColliderComponent>();
    u32 colCount = colPool.Size();
    f32 minTOI = 1.0f;

    for (u32 i = 0; i < colCount; i++) {
        Entity other = colPool.GetEntity(i);
        if (other == e) continue;

        ColliderComponent& otherCol = colPool.Data(i);
        auto* otherTr = world.GetComponent<TransformComponent>(other);
        if (!otherTr) continue;

        AABB otherAABB = otherCol.GetWorldAABB(*otherTr);

        // 粗筛: 扫掠 AABB 与目标 AABB 是否相交
        if (!Collision::TestAABB(sweepAABB, otherAABB)) continue;

        // 精确 TOI 计算: Minkowski AABB 扩展 + 射线投射
        // 将 B 扩展 A 的半尺寸，然后对扩展后的 B 做射线检测
        AABB minkowski;
        glm::vec3 halfA = startAABB.HalfSize();
        minkowski.Min = otherAABB.Min - halfA;
        minkowski.Max = otherAABB.Max + halfA;

        Ray sweepRay;
        sweepRay.Origin = startAABB.Center();
        sweepRay.Direction = dir;

        HitResult hit = Collision::RaycastAABB(sweepRay, minkowski);
        if (hit.Hit && hit.Distance >= 0.0f && hit.Distance <= dispLen) {
            f32 toi = hit.Distance / dispLen;
            if (toi < minTOI) {
                minTOI = toi;
                result.Hit = true;
                result.TOI = toi;
                result.HitPoint = hit.Point;
                result.HitNormal = hit.Normal;
                result.HitEntity = other;
            }
        }
    }

    return result;
}

void PhysicsWorld::PerformCCD(ECSWorld& world, f32 dt) {
    auto& rbPool = world.GetComponentArray<RigidBodyComponent>();
    u32 count = rbPool.Size();

    for (u32 i = 0; i < count; i++) {
        RigidBodyComponent& rb = rbPool.Data(i);
        if (rb.IsStatic) continue;

        Entity e = rbPool.GetEntity(i);
        auto* col = world.GetComponent<ColliderComponent>(e);
        if (!col || !col->UseCCD) continue;

        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) continue;

        // 高速检测阈值: 速度 * dt > AABB 半尺寸的最小维度
        glm::vec3 displacement = rb.Velocity * dt;
        f32 dispLen = glm::length(displacement);
        AABB worldAABB = col->GetWorldAABB(*tr);
        glm::vec3 halfSize = worldAABB.HalfSize();
        f32 minHalf = std::min({halfSize.x, halfSize.y, halfSize.z});

        if (dispLen < minHalf) continue; // 低速不需要 CCD

        CCDResult ccd = SweepTest(world, e, displacement);
        if (ccd.Hit && ccd.TOI < 1.0f) {
            // 回退到碰撞点（留一点间隙避免穿透）
            f32 safeT = std::max(0.0f, ccd.TOI - 0.01f);
            glm::vec3 correction = displacement * (safeT - 1.0f);
            tr->X += correction.x;
            tr->Y += correction.y;
            tr->Z += correction.z;

            // 反弹速度
            f32 vn = glm::dot(rb.Velocity, ccd.HitNormal);
            if (vn < 0) {
                rb.Velocity -= ccd.HitNormal * vn * (1.0f + rb.Restitution);
            }
        }
    }
}

// ── 碰撞检测（SoA + SpatialHash）──────────────────────────

void PhysicsWorld::DetectCollisions(ECSWorld& world) {
    s_Pairs.clear();

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
        for (size_t i = 0; i < entities.size(); i++) {
            for (size_t j = i + 1; j < entities.size(); j++) {
                narrowPhase(i, j);
            }
        }
    } else {
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

// ── 碰撞响应（基于质量的冲量模型）──────────────────────────

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

        // 位置分离（按质量比例）
        f32 invMassA = staticA ? 0.0f : 1.0f / rbA->Mass;
        f32 invMassB = staticB ? 0.0f : 1.0f / rbB->Mass;
        f32 totalInvMass = invMassA + invMassB;

        if (totalInvMass > 0) {
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

        // 冲量（动量守恒 + 恢复系数）
        glm::vec3 velA = staticA ? glm::vec3(0) : rbA->Velocity;
        glm::vec3 velB = staticB ? glm::vec3(0) : rbB->Velocity;
        glm::vec3 relVel = velA - velB;
        f32 velAlongNormal = glm::dot(relVel, pair.Normal);

        if (velAlongNormal > 0) continue; // 正在分离

        f32 restitution = 0.3f;
        if (rbA && rbB)      restitution = std::min(rbA->Restitution, rbB->Restitution);
        else if (rbA)        restitution = rbA->Restitution;
        else if (rbB)        restitution = rbB->Restitution;

        f32 j = -(1.0f + restitution) * velAlongNormal / totalInvMass;
        glm::vec3 impulse = pair.Normal * j;
        if (rbA && !rbA->IsStatic) rbA->Velocity += impulse * invMassA;
        if (rbB && !rbB->IsStatic) rbB->Velocity -= impulse * invMassB;

        // 库仑摩擦（切向）
        velA = staticA ? glm::vec3(0) : rbA->Velocity;
        velB = staticB ? glm::vec3(0) : rbB->Velocity;
        relVel = velA - velB;
        glm::vec3 tangent = relVel - pair.Normal * glm::dot(relVel, pair.Normal);
        f32 tangentLen = glm::length(tangent);
        if (tangentLen > 1e-6f) {
            tangent /= tangentLen;
            f32 jt = -glm::dot(relVel, tangent) / totalInvMass;
            f32 friction = 0.5f;
            if (rbA) friction = rbA->Friction;
            if (rbB) friction = std::min(friction, rbB->Friction);
            f32 maxFriction = friction * std::abs(j);
            jt = glm::clamp(jt, -maxFriction, maxFriction);

            glm::vec3 frictionImpulse = tangent * jt;
            if (rbA && !rbA->IsStatic) rbA->Velocity += frictionImpulse * invMassA;
            if (rbB && !rbB->IsStatic) rbB->Velocity -= frictionImpulse * invMassB;
        }
    }
}

// ── 约束求解（迭代投影法）────────────────────────────────

void PhysicsWorld::SolveConstraints(ECSWorld& world, f32 dt) {
    if (s_Constraints.empty()) return;

    for (i32 iter = 0; iter < CONSTRAINT_ITERATIONS; iter++) {
        for (auto& c : s_Constraints) {
            if (!c.Enabled) continue;
            if (c.EntityA == INVALID_ENTITY || c.EntityB == INVALID_ENTITY) continue;

            auto* trA = world.GetComponent<TransformComponent>(c.EntityA);
            auto* trB = world.GetComponent<TransformComponent>(c.EntityB);
            if (!trA || !trB) continue;

            auto* rbA = world.GetComponent<RigidBodyComponent>(c.EntityA);
            auto* rbB = world.GetComponent<RigidBodyComponent>(c.EntityB);

            bool staticA = (!rbA || rbA->IsStatic);
            bool staticB = (!rbB || rbB->IsStatic);
            if (staticA && staticB) continue;

            f32 invMassA = staticA ? 0.0f : 1.0f / rbA->Mass;
            f32 invMassB = staticB ? 0.0f : 1.0f / rbB->Mass;
            f32 totalInvMass = invMassA + invMassB;
            if (totalInvMass < 1e-8f) continue;

            // 世界空间锚点
            glm::vec3 worldAnchorA = glm::vec3(trA->X, trA->Y, trA->Z) + c.AnchorA;
            glm::vec3 worldAnchorB = glm::vec3(trB->X, trB->Y, trB->Z) + c.AnchorB;

            switch (c.Type) {

            // ── 距离约束 ─────────────────────────────────
            case ConstraintType::Distance:
            case ConstraintType::PointToPoint: {
                glm::vec3 delta = worldAnchorB - worldAnchorA;
                f32 currentDist = glm::length(delta);
                f32 targetDist = (c.Type == ConstraintType::PointToPoint) 
                                 ? 0.0f : c.Distance;

                if (currentDist < 1e-6f) break;

                glm::vec3 dir = delta / currentDist;
                f32 error = currentDist - targetDist;

                // 位置修正（Baumgarte stabilization β = 0.2）
                f32 baumgarte = 0.2f;
                glm::vec3 correction = dir * error * baumgarte / totalInvMass;

                if (!staticA) {
                    trA->X += correction.x * invMassA;
                    trA->Y += correction.y * invMassA;
                    trA->Z += correction.z * invMassA;
                }
                if (!staticB) {
                    trB->X -= correction.x * invMassB;
                    trB->Y -= correction.y * invMassB;
                    trB->Z -= correction.z * invMassB;
                }

                // 速度修正
                if (rbA || rbB) {
                    glm::vec3 relVel = (staticA ? glm::vec3(0) : rbA->Velocity)
                                     - (staticB ? glm::vec3(0) : rbB->Velocity);
                    f32 velAlongDir = glm::dot(relVel, dir);
                    glm::vec3 velCorrection = dir * velAlongDir / totalInvMass;

                    if (rbA && !rbA->IsStatic)
                        rbA->Velocity -= velCorrection * invMassA;
                    if (rbB && !rbB->IsStatic)
                        rbB->Velocity += velCorrection * invMassB;
                }
                break;
            }

            // ── 弹簧约束 ─────────────────────────────────
            case ConstraintType::Spring: {
                glm::vec3 delta = worldAnchorB - worldAnchorA;
                f32 currentDist = glm::length(delta);
                if (currentDist < 1e-6f) break;

                glm::vec3 dir = delta / currentDist;
                f32 stretch = currentDist - c.Distance;

                // 弹簧力: F = -k*x - b*v
                glm::vec3 relVel = (staticA ? glm::vec3(0) : rbA->Velocity)
                                 - (staticB ? glm::vec3(0) : rbB->Velocity);
                f32 velAlongDir = glm::dot(relVel, dir);

                f32 forceMag = c.Stiffness * stretch + c.Damping * velAlongDir;
                glm::vec3 force = dir * forceMag;

                if (rbA && !rbA->IsStatic)
                    rbA->Velocity += force * (invMassA * dt);
                if (rbB && !rbB->IsStatic)
                    rbB->Velocity -= force * (invMassB * dt);
                break;
            }

            // ── 铰链约束 ─────────────────────────────────
            case ConstraintType::Hinge: {
                // 先满足点对点约束（保持锚点重合）
                glm::vec3 delta = worldAnchorB - worldAnchorA;
                f32 currentDist = glm::length(delta);
                if (currentDist > 1e-4f) {
                    glm::vec3 dir = delta / currentDist;
                    f32 baumgarte = 0.3f;
                    glm::vec3 correction = dir * currentDist * baumgarte / totalInvMass;

                    if (!staticA) {
                        trA->X += correction.x * invMassA;
                        trA->Y += correction.y * invMassA;
                        trA->Z += correction.z * invMassA;
                    }
                    if (!staticB) {
                        trB->X -= correction.x * invMassB;
                        trB->Y -= correction.y * invMassB;
                        trB->Z -= correction.z * invMassB;
                    }
                }

                // 限制角速度到铰链轴方向
                glm::vec3 axis = glm::normalize(c.HingeAxis);
                if (rbA && !rbA->IsStatic) {
                    // 只保留沿铰链轴的角速度分量
                    f32 angVelAlong = glm::dot(rbA->AngularVelocity, axis);
                    rbA->AngularVelocity = axis * angVelAlong;
                }
                if (rbB && !rbB->IsStatic) {
                    f32 angVelAlong = glm::dot(rbB->AngularVelocity, axis);
                    rbB->AngularVelocity = axis * angVelAlong;
                }
                break;
            }

            } // switch
        } // for constraints
    } // for iterations
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
                if (std::abs(rb.Velocity.y) < 0.1f) {
                    rb.Velocity.y = 0;
                }
            }
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

// ── 力 / 冲量 / 力矩 ────────────────────────────────────────

void PhysicsWorld::AddForce(ECSWorld& world, Entity e, const glm::vec3& force) {
    auto* rb = world.GetComponent<RigidBodyComponent>(e);
    if (!rb || rb->IsStatic || rb->Mass <= 0) return;
    rb->Acceleration += force / rb->Mass;
}

void PhysicsWorld::AddImpulse(ECSWorld& world, Entity e, const glm::vec3& impulse) {
    auto* rb = world.GetComponent<RigidBodyComponent>(e);
    if (!rb || rb->IsStatic || rb->Mass <= 0) return;
    rb->Velocity += impulse / rb->Mass;
}

void PhysicsWorld::AddTorque(ECSWorld& world, Entity e, const glm::vec3& torque) {
    auto* rb = world.GetComponent<RigidBodyComponent>(e);
    if (!rb || rb->IsStatic || rb->Mass <= 0) return;
    rb->AngularVelocity += torque / rb->Mass;
}

// ── 约束管理 ────────────────────────────────────────────────

u32 PhysicsWorld::AddConstraint(const Constraint& c) {
    u32 id = s_ConstraintIdCounter++;
    s_Constraints.push_back(c);
    return id;
}

void PhysicsWorld::RemoveConstraint(u32 id) {
    if (id < s_Constraints.size()) {
        s_Constraints.erase(s_Constraints.begin() + id);
    }
}

Constraint* PhysicsWorld::GetConstraint(u32 id) {
    if (id < s_Constraints.size()) return &s_Constraints[id];
    return nullptr;
}

void PhysicsWorld::ClearConstraints() {
    s_Constraints.clear();
    s_ConstraintIdCounter = 0;
}

u32 PhysicsWorld::GetConstraintCount() {
    return (u32)s_Constraints.size();
}

} // namespace Engine
