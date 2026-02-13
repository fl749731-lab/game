#include "engine/physics/physics_world.h"
#include "engine/core/event.h"
#include "engine/core/log.h"

#include <algorithm>
#include <cmath>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

std::vector<CollisionPair> PhysicsWorld::s_Pairs;
CollisionCallback PhysicsWorld::s_Callback = nullptr;
CollisionEventCallback PhysicsWorld::s_EventCallback = nullptr;
f32 PhysicsWorld::s_GroundHeight = 0.0f;
std::vector<Constraint> PhysicsWorld::s_Constraints;
u32 PhysicsWorld::s_ConstraintIdCounter = 0;
f32 PhysicsWorld::s_FixedTimestep = 1.0f / 60.0f;
f32 PhysicsWorld::s_Accumulator = 0.0f;
std::unordered_set<PhysicsWorld::PairKey, PhysicsWorld::PairHash> PhysicsWorld::s_PreviousPairs;
std::unordered_set<PhysicsWorld::PairKey, PhysicsWorld::PairHash> PhysicsWorld::s_CurrentPairs;
std::vector<CollisionEventData> PhysicsWorld::s_CollisionEvents;

// ── 固定步长累积器 ──────────────────────────────────────────

void PhysicsWorld::Update(ECSWorld& world, f32 frameTime) {
    // 防止帧时间过大导致死循环（断点调试等）
    frameTime = std::min(frameTime, 0.25f);
    s_Accumulator += frameTime;

    while (s_Accumulator >= s_FixedTimestep) {
        Step(world, s_FixedTimestep);
        s_Accumulator -= s_FixedTimestep;
    }
}

void PhysicsWorld::SetFixedTimestep(f32 dt) { s_FixedTimestep = std::max(dt, 1.0f/240.0f); }
f32  PhysicsWorld::GetFixedTimestep() { return s_FixedTimestep; }

// ── 主更新（单步）────────────────────────────────────────

void PhysicsWorld::Step(ECSWorld& world, f32 dt) {
    UpdateSleep(world, dt);
    IntegrateForces(world, dt);
    PerformCCD(world, dt);
    DetectCollisions(world);
    UpdateCollisionEvents();
    ResolveCollisions(world);
    SolveConstraints(world, dt);
    ResolveGroundCollisions(world);
    UpdateCharacterControllers(world, dt);
}

// ── 休眠系统 ────────────────────────────────────────────────

void PhysicsWorld::UpdateSleep(ECSWorld& world, f32 dt) {
    auto& rbPool = world.GetComponentArray<RigidBodyComponent>();
    u32 count = rbPool.Size();

    for (u32 i = 0; i < count; i++) {
        RigidBodyComponent& rb = rbPool.Data(i);
        if (rb.IsStatic || !rb.CanSleep) continue;

        f32 linearSpeed = glm::length(rb.Velocity);
        f32 angularSpeed = glm::length(rb.AngularVelocity);
        f32 totalEnergy = linearSpeed + angularSpeed;

        if (totalEnergy < rb.SleepThreshold) {
            rb.SleepTimer += dt;
            if (rb.SleepTimer >= rb.SleepDelay) {
                rb.IsSleeping = true;
                rb.Velocity = {0, 0, 0};
                rb.AngularVelocity = {0, 0, 0};
            }
        } else {
            rb.SleepTimer = 0.0f;
            rb.IsSleeping = false;
        }
    }
}

// ── 力积分 ──────────────────────────────────────────────────

void PhysicsWorld::IntegrateForces(ECSWorld& world, f32 dt) {
    auto& rbPool = world.GetComponentArray<RigidBodyComponent>();
    u32 count = rbPool.Size();

    for (u32 i = 0; i < count; i++) {
        RigidBodyComponent& rb = rbPool.Data(i);
        if (rb.IsStatic || rb.IsSleeping) continue;

        Entity e = rbPool.GetEntity(i);
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) continue;

        if (rb.UseGravity) rb.Velocity += rb.GravityOverride * dt;
        rb.Velocity += rb.Acceleration * dt;
        rb.Velocity *= (1.0f - rb.LinearDamping * dt);

        tr->X += rb.Velocity.x * dt;
        tr->Y += rb.Velocity.y * dt;
        tr->Z += rb.Velocity.z * dt;

        if (glm::length(rb.AngularVelocity) > 1e-6f) {
            tr->RotX += glm::degrees(rb.AngularVelocity.x) * dt;
            tr->RotY += glm::degrees(rb.AngularVelocity.y) * dt;
            tr->RotZ += glm::degrees(rb.AngularVelocity.z) * dt;
            rb.AngularVelocity *= (1.0f - rb.AngularDamping * dt);
        }

        rb.Acceleration = {0, 0, 0};
    }
}

// ── 通用碰撞检测（根据形状分派）──────────────────────────

bool PhysicsWorld::TestColliders(const ColliderComponent& colA, const TransformComponent& trA,
                                  const ColliderComponent& colB, const TransformComponent& trB,
                                  glm::vec3& outNormal, f32& outPenetration) {
    // 碰撞层过滤
    if (!Collision::LayersCanCollide(colA.Layer, colA.Mask, colB.Layer, colB.Mask))
        return false;

    auto shapeA = colA.Shape;
    auto shapeB = colB.Shape;

    // Box vs Box
    if (shapeA == ColliderShape::Box && shapeB == ColliderShape::Box) {
        AABB a, b;
        a.Min = colA.LocalBounds.Min * glm::vec3(trA.ScaleX, trA.ScaleY, trA.ScaleZ) + glm::vec3(trA.X, trA.Y, trA.Z);
        a.Max = colA.LocalBounds.Max * glm::vec3(trA.ScaleX, trA.ScaleY, trA.ScaleZ) + glm::vec3(trA.X, trA.Y, trA.Z);
        b.Min = colB.LocalBounds.Min * glm::vec3(trB.ScaleX, trB.ScaleY, trB.ScaleZ) + glm::vec3(trB.X, trB.Y, trB.Z);
        b.Max = colB.LocalBounds.Max * glm::vec3(trB.ScaleX, trB.ScaleY, trB.ScaleZ) + glm::vec3(trB.X, trB.Y, trB.Z);
        return Collision::TestAABB(a, b, outNormal, outPenetration);
    }

    // Sphere vs Sphere
    if (shapeA == ColliderShape::Sphere && shapeB == ColliderShape::Sphere) {
        Sphere a = colA.GetWorldSphere(trA);
        Sphere b = colB.GetWorldSphere(trB);
        return Collision::TestSpheres(a, b, outNormal, outPenetration);
    }

    // Sphere vs Box
    if (shapeA == ColliderShape::Sphere && shapeB == ColliderShape::Box) {
        Sphere s = colA.GetWorldSphere(trA);
        AABB aabb = colB.GetWorldAABB(trB);
        return Collision::TestSphereAABB(s, aabb, outNormal, outPenetration);
    }
    if (shapeA == ColliderShape::Box && shapeB == ColliderShape::Sphere) {
        Sphere s = colB.GetWorldSphere(trB);
        AABB aabb = colA.GetWorldAABB(trA);
        bool hit = Collision::TestSphereAABB(s, aabb, outNormal, outPenetration);
        outNormal = -outNormal; // 反转法线方向
        return hit;
    }

    // Capsule vs Capsule
    if (shapeA == ColliderShape::Capsule && shapeB == ColliderShape::Capsule) {
        Capsule a = colA.GetWorldCapsule(trA);
        Capsule b = colB.GetWorldCapsule(trB);
        return Collision::TestCapsules(a, b, outNormal, outPenetration);
    }

    // Capsule vs Box
    if (shapeA == ColliderShape::Capsule && shapeB == ColliderShape::Box) {
        Capsule cap = colA.GetWorldCapsule(trA);
        AABB aabb = colB.GetWorldAABB(trB);
        return Collision::TestCapsuleAABB(cap, aabb, outNormal, outPenetration);
    }
    if (shapeA == ColliderShape::Box && shapeB == ColliderShape::Capsule) {
        Capsule cap = colB.GetWorldCapsule(trB);
        AABB aabb = colA.GetWorldAABB(trA);
        bool hit = Collision::TestCapsuleAABB(cap, aabb, outNormal, outPenetration);
        outNormal = -outNormal;
        return hit;
    }

    // Capsule vs Sphere
    if (shapeA == ColliderShape::Capsule && shapeB == ColliderShape::Sphere) {
        Capsule cap = colA.GetWorldCapsule(trA);
        Sphere sph = colB.GetWorldSphere(trB);
        return Collision::TestCapsuleSphere(cap, sph, outNormal, outPenetration);
    }
    if (shapeA == ColliderShape::Sphere && shapeB == ColliderShape::Capsule) {
        Capsule cap = colB.GetWorldCapsule(trB);
        Sphere sph = colA.GetWorldSphere(trA);
        bool hit = Collision::TestCapsuleSphere(cap, sph, outNormal, outPenetration);
        outNormal = -outNormal;
        return hit;
    }

    // 后备: AABB vs AABB
    AABB a = colA.GetWorldAABB(trA);
    AABB b = colB.GetWorldAABB(trB);
    return Collision::TestAABB(a, b, outNormal, outPenetration);
}

// ── CCD ─────────────────────────────────────────────────────

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

    AABB endAABB;
    endAABB.Min = startAABB.Min + displacement;
    endAABB.Max = startAABB.Max + displacement;

    AABB sweepAABB;
    sweepAABB.Min = glm::min(startAABB.Min, endAABB.Min);
    sweepAABB.Max = glm::max(startAABB.Max, endAABB.Max);

    auto& colPool = world.GetComponentArray<ColliderComponent>();
    u32 colCount = colPool.Size();
    f32 minTOI = 1.0f;

    for (u32 i = 0; i < colCount; i++) {
        Entity other = colPool.GetEntity(i);
        if (other == e) continue;

        ColliderComponent& otherCol = colPool.Data(i);
        // 碰撞层过滤
        if (!Collision::LayersCanCollide(col->Layer, col->Mask, otherCol.Layer, otherCol.Mask))
            continue;

        auto* otherTr = world.GetComponent<TransformComponent>(other);
        if (!otherTr) continue;

        AABB otherAABB = otherCol.GetWorldAABB(*otherTr);
        if (!Collision::TestAABB(sweepAABB, otherAABB)) continue;

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
        if (rb.IsStatic || rb.IsSleeping) continue;

        Entity e = rbPool.GetEntity(i);
        auto* col = world.GetComponent<ColliderComponent>(e);
        if (!col || !col->UseCCD) continue;

        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) continue;

        glm::vec3 displacement = rb.Velocity * dt;
        f32 dispLen = glm::length(displacement);
        AABB worldAABB = col->GetWorldAABB(*tr);
        glm::vec3 halfSize = worldAABB.HalfSize();
        f32 minHalf = std::min({halfSize.x, halfSize.y, halfSize.z});

        if (dispLen < minHalf) continue;

        CCDResult ccd = SweepTest(world, e, displacement);
        if (ccd.Hit && ccd.TOI < 1.0f) {
            f32 safeT = std::max(0.0f, ccd.TOI - 0.01f);
            glm::vec3 correction = displacement * (safeT - 1.0f);
            tr->X += correction.x;
            tr->Y += correction.y;
            tr->Z += correction.z;

            f32 vn = glm::dot(rb.Velocity, ccd.HitNormal);
            if (vn < 0) {
                rb.Velocity -= ccd.HitNormal * vn * (1.0f + rb.Restitution);
            }
            rb.WakeUp();
        }
    }
}

// ── 碰撞检测 ────────────────────────────────────────────────

void PhysicsWorld::DetectCollisions(ECSWorld& world) {
    s_Pairs.clear();
    s_CurrentPairs.clear();

    auto& colPool = world.GetComponentArray<ColliderComponent>();
    u32 colCount = colPool.Size();

    struct ColEntity {
        Entity e;
        AABB worldAABB;
    };
    std::vector<ColEntity> entities;
    entities.reserve(colCount);

    for (u32 i = 0; i < colCount; i++) {
        ColliderComponent& col = colPool.Data(i);
        Entity e = colPool.GetEntity(i);
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) continue;

        // 休眠跳过
        auto* rb = world.GetComponent<RigidBodyComponent>(e);
        if (rb && rb->IsSleeping) continue;

        entities.push_back({e, col.GetWorldAABB(*tr)});
    }

    auto narrowPhase = [&](size_t i, size_t j) {
        auto* colA = world.GetComponent<ColliderComponent>(entities[i].e);
        auto* colB = world.GetComponent<ColliderComponent>(entities[j].e);
        auto* trA = world.GetComponent<TransformComponent>(entities[i].e);
        auto* trB = world.GetComponent<TransformComponent>(entities[j].e);
        if (!colA || !colB || !trA || !trB) return;

        glm::vec3 normal;
        f32 penetration;
        if (TestColliders(*colA, *trA, *colB, *trB, normal, penetration)) {
            CollisionPair pair;
            pair.EntityA = entities[i].e;
            pair.EntityB = entities[j].e;
            pair.Normal = normal;
            pair.Penetration = penetration;
            s_Pairs.push_back(pair);
            s_CurrentPairs.insert({entities[i].e, entities[j].e});

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
                // AABB 粗筛
                if (Collision::TestAABB(entities[i].worldAABB, entities[j].worldAABB)) {
                    narrowPhase(i, j);
                }
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

// ── 碰撞事件更新 (Enter/Stay/Exit) ─────────────────────────

void PhysicsWorld::UpdateCollisionEvents() {
    s_CollisionEvents.clear();

    // Enter: 本帧有、上帧没有
    for (auto& pair : s_CurrentPairs) {
        if (s_PreviousPairs.find(pair) == s_PreviousPairs.end()) {
            s_CollisionEvents.push_back({pair.a, pair.b, CollisionState::Enter});
        } else {
            s_CollisionEvents.push_back({pair.a, pair.b, CollisionState::Stay});
        }
    }

    // Exit: 上帧有、本帧没有
    for (auto& pair : s_PreviousPairs) {
        if (s_CurrentPairs.find(pair) == s_CurrentPairs.end()) {
            s_CollisionEvents.push_back({pair.a, pair.b, CollisionState::Exit});
        }
    }

    // 通知回调
    if (s_EventCallback) {
        for (auto& evt : s_CollisionEvents) {
            s_EventCallback(evt);
        }
    }

    // 交换帧
    s_PreviousPairs = s_CurrentPairs;
}

// ── 碰撞响应 ────────────────────────────────────────────────

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

        // 唤醒碰撞中的休眠物体
        if (rbA && rbA->IsSleeping) rbA->WakeUp();
        if (rbB && rbB->IsSleeping) rbB->WakeUp();

        // 使用碰撞双方材质的混合值
        f32 restitution = std::min(colA->Material.Restitution, colB->Material.Restitution);
        f32 friction = std::sqrt(colA->Material.Friction * colB->Material.Friction);

        f32 invMassA = staticA ? 0.0f : 1.0f / rbA->Mass;
        f32 invMassB = staticB ? 0.0f : 1.0f / rbB->Mass;
        f32 totalInvMass = invMassA + invMassB;

        // 位置分离
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

        // 冲量
        glm::vec3 velA = staticA ? glm::vec3(0) : rbA->Velocity;
        glm::vec3 velB = staticB ? glm::vec3(0) : rbB->Velocity;
        glm::vec3 relVel = velA - velB;
        f32 velAlongNormal = glm::dot(relVel, pair.Normal);
        if (velAlongNormal > 0) continue;

        f32 j = -(1.0f + restitution) * velAlongNormal / totalInvMass;
        glm::vec3 impulse = pair.Normal * j;
        if (rbA && !rbA->IsStatic) rbA->Velocity += impulse * invMassA;
        if (rbB && !rbB->IsStatic) rbB->Velocity -= impulse * invMassB;

        // 摩擦
        velA = staticA ? glm::vec3(0) : rbA->Velocity;
        velB = staticB ? glm::vec3(0) : rbB->Velocity;
        relVel = velA - velB;
        glm::vec3 tangent = relVel - pair.Normal * glm::dot(relVel, pair.Normal);
        f32 tangentLen = glm::length(tangent);
        if (tangentLen > 1e-6f) {
            tangent /= tangentLen;
            f32 jt = -glm::dot(relVel, tangent) / totalInvMass;
            f32 maxFriction = friction * std::abs(j);
            jt = glm::clamp(jt, -maxFriction, maxFriction);

            glm::vec3 frictionImpulse = tangent * jt;
            if (rbA && !rbA->IsStatic) rbA->Velocity += frictionImpulse * invMassA;
            if (rbB && !rbB->IsStatic) rbB->Velocity -= frictionImpulse * invMassB;
        }
    }
}

// ── 约束求解 ────────────────────────────────────────────────

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

            glm::vec3 worldAnchorA = glm::vec3(trA->X, trA->Y, trA->Z) + c.AnchorA;
            glm::vec3 worldAnchorB = glm::vec3(trB->X, trB->Y, trB->Z) + c.AnchorB;

            switch (c.Type) {
            case ConstraintType::Distance:
            case ConstraintType::PointToPoint: {
                glm::vec3 delta = worldAnchorB - worldAnchorA;
                f32 currentDist = glm::length(delta);
                f32 targetDist = (c.Type == ConstraintType::PointToPoint) ? 0.0f : c.Distance;
                if (currentDist < 1e-6f) break;

                glm::vec3 dir = delta / currentDist;
                f32 error = currentDist - targetDist;
                f32 baumgarte = 0.2f;
                glm::vec3 correction = dir * error * baumgarte / totalInvMass;

                if (!staticA) { trA->X += correction.x * invMassA; trA->Y += correction.y * invMassA; trA->Z += correction.z * invMassA; }
                if (!staticB) { trB->X -= correction.x * invMassB; trB->Y -= correction.y * invMassB; trB->Z -= correction.z * invMassB; }

                if (rbA || rbB) {
                    glm::vec3 relVel = (staticA ? glm::vec3(0) : rbA->Velocity) - (staticB ? glm::vec3(0) : rbB->Velocity);
                    f32 velAlongDir = glm::dot(relVel, dir);
                    glm::vec3 velCorrection = dir * velAlongDir / totalInvMass;
                    if (rbA && !rbA->IsStatic) rbA->Velocity -= velCorrection * invMassA;
                    if (rbB && !rbB->IsStatic) rbB->Velocity += velCorrection * invMassB;
                }
                break;
            }

            case ConstraintType::Spring: {
                glm::vec3 delta = worldAnchorB - worldAnchorA;
                f32 currentDist = glm::length(delta);
                if (currentDist < 1e-6f) break;

                glm::vec3 dir = delta / currentDist;
                f32 stretch = currentDist - c.Distance;
                glm::vec3 relVel = (staticA ? glm::vec3(0) : rbA->Velocity) - (staticB ? glm::vec3(0) : rbB->Velocity);
                f32 velAlongDir = glm::dot(relVel, dir);
                f32 forceMag = c.Stiffness * stretch + c.Damping * velAlongDir;
                glm::vec3 force = dir * forceMag;

                if (rbA && !rbA->IsStatic) rbA->Velocity += force * (invMassA * dt);
                if (rbB && !rbB->IsStatic) rbB->Velocity -= force * (invMassB * dt);
                break;
            }

            case ConstraintType::Hinge: {
                glm::vec3 delta = worldAnchorB - worldAnchorA;
                f32 currentDist = glm::length(delta);
                if (currentDist > 1e-4f) {
                    glm::vec3 dir = delta / currentDist;
                    glm::vec3 correction = dir * currentDist * 0.3f / totalInvMass;
                    if (!staticA) { trA->X += correction.x * invMassA; trA->Y += correction.y * invMassA; trA->Z += correction.z * invMassA; }
                    if (!staticB) { trB->X -= correction.x * invMassB; trB->Y -= correction.y * invMassB; trB->Z -= correction.z * invMassB; }
                }

                glm::vec3 axis = glm::normalize(c.HingeAxis);
                if (rbA && !rbA->IsStatic) {
                    f32 angVelAlong = glm::dot(rbA->AngularVelocity, axis);
                    rbA->AngularVelocity = axis * angVelAlong;
                }
                if (rbB && !rbB->IsStatic) {
                    f32 angVelAlong = glm::dot(rbB->AngularVelocity, axis);
                    rbB->AngularVelocity = axis * angVelAlong;
                }
                break;
            }
            }
        }
    }
}

// ── 地面碰撞 ────────────────────────────────────────────────

void PhysicsWorld::ResolveGroundCollisions(ECSWorld& world) {
    auto& rbPool = world.GetComponentArray<RigidBodyComponent>();
    u32 count = rbPool.Size();

    for (u32 i = 0; i < count; i++) {
        RigidBodyComponent& rb = rbPool.Data(i);
        if (rb.IsStatic || rb.IsSleeping) continue;

        Entity e = rbPool.GetEntity(i);
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) continue;

        auto* col = world.GetComponent<ColliderComponent>(e);
        f32 bottom = tr->Y;
        if (col) {
            AABB worldAABB = col->GetWorldAABB(*tr);
            bottom = worldAABB.Min.y;
        }

        if (bottom < s_GroundHeight) {
            f32 penetration = s_GroundHeight - bottom;
            tr->Y += penetration;
            if (rb.Velocity.y < 0) {
                rb.Velocity.y = -rb.Velocity.y * rb.Restitution;
                if (std::abs(rb.Velocity.y) < 0.1f) rb.Velocity.y = 0;
            }
            f32 frictionDecay = 1.0f - rb.Friction * 0.1f;
            rb.Velocity.x *= frictionDecay;
            rb.Velocity.z *= frictionDecay;
        }
    }
}

// ── 角色控制器 ──────────────────────────────────────────────

void PhysicsWorld::UpdateCharacterControllers(ECSWorld& world, f32 dt) {
    auto& ccPool = world.GetComponentArray<CharacterControllerComponent>();
    u32 count = ccPool.Size();

    for (u32 i = 0; i < count; i++) {
        CharacterControllerComponent& cc = ccPool.Data(i);
        Entity e = ccPool.GetEntity(i);
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) continue;

        // 胶囊底部
        f32 halfH = (cc.Height - 2.0f * cc.Radius) * 0.5f;
        f32 feetY = tr->Y - halfH - cc.Radius;

        // 地面检测 — 脚底是否接触地面
        cc.IsGrounded = (feetY <= s_GroundHeight + 0.05f);

        // 重力
        if (cc.IsGrounded) {
            if (cc.VerticalSpeed < 0) cc.VerticalSpeed = 0;

            // 跳跃
            if (cc.WantsJump) {
                cc.VerticalSpeed = cc.JumpForce;
                cc.IsGrounded = false;
                cc.WantsJump = false;
            }
        } else {
            cc.VerticalSpeed += cc.Gravity * dt;
        }

        // 水平移动
        glm::vec3 moveDir = cc.MoveDir;
        f32 moveLen = glm::length(moveDir);
        if (moveLen > 1e-4f) {
            moveDir /= moveLen; // 归一化
            if (moveLen > 1.0f) moveLen = 1.0f;
            tr->X += moveDir.x * cc.MoveSpeed * moveLen * dt;
            tr->Z += moveDir.z * cc.MoveSpeed * moveLen * dt;
        }

        // 垂直移动
        tr->Y += cc.VerticalSpeed * dt;

        // 地面钳制
        feetY = tr->Y - halfH - cc.Radius;
        if (feetY < s_GroundHeight) {
            tr->Y += s_GroundHeight - feetY;
            cc.VerticalSpeed = 0;
            cc.IsGrounded = true;
        }

        // 台阶检测（简化版：如果前方有矮障碍物，自动抬升）
        if (moveLen > 1e-4f && cc.IsGrounded) {
            // 检测前方障碍
            Ray stepRay;
            stepRay.Origin = {tr->X, tr->Y - halfH + cc.StepHeight, tr->Z};
            stepRay.Direction = {moveDir.x, 0, moveDir.z};
            if (glm::length(stepRay.Direction) > 1e-4f)
                stepRay.Direction = glm::normalize(stepRay.Direction);

            Entity hitEnt;
            HitResult hit = Raycast(world, stepRay, &hitEnt, cc.Mask);
            if (hit.Hit && hit.Distance < cc.Radius * 2.0f) {
                // 检测头顶空间
                Ray upRay;
                upRay.Origin = {tr->X + moveDir.x * cc.Radius * 2.0f,
                                tr->Y - halfH, tr->Z + moveDir.z * cc.Radius * 2.0f};
                upRay.Direction = {0, 1, 0};
                HitResult upHit = Raycast(world, upRay, nullptr, cc.Mask);
                if (!upHit.Hit || upHit.Distance > cc.StepHeight * 2.0f) {
                    tr->Y += cc.StepHeight;
                }
            }
        }

        // 清零移动请求
        cc.MoveDir = {0, 0, 0};
        cc.WantsJump = false;
    }
}

// ── 射线检测 ────────────────────────────────────────────────

HitResult PhysicsWorld::Raycast(ECSWorld& world, const Ray& ray,
                                 Entity* outEntity, u16 layerMask) {
    HitResult closest;
    closest.Distance = 1e30f;

    auto& colPool = world.GetComponentArray<ColliderComponent>();
    u32 count = colPool.Size();

    for (u32 i = 0; i < count; i++) {
        ColliderComponent& col = colPool.Data(i);
        // 层过滤
        if ((col.Layer & layerMask) == 0) continue;

        Entity e = colPool.GetEntity(i);
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) continue;

        HitResult hit;
        switch (col.Shape) {
        case ColliderShape::Box:
            hit = Collision::RaycastAABB(ray, col.GetWorldAABB(*tr));
            break;
        case ColliderShape::Sphere:
            hit = Collision::RaycastSphere(ray, col.GetWorldSphere(*tr));
            break;
        case ColliderShape::Capsule:
            hit = Collision::RaycastCapsule(ray, col.GetWorldCapsule(*tr));
            break;
        }

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
void PhysicsWorld::SetCollisionEventCallback(CollisionEventCallback cb) { s_EventCallback = cb; }
const std::vector<CollisionPair>& PhysicsWorld::GetCollisionPairs() { return s_Pairs; }
const std::vector<CollisionEventData>& PhysicsWorld::GetCollisionEvents() { return s_CollisionEvents; }
void PhysicsWorld::SetGroundPlane(f32 height) { s_GroundHeight = height; }
f32  PhysicsWorld::GetGroundPlane() { return s_GroundHeight; }

// ── 力 / 冲量 / 力矩 ────────────────────────────────────────

void PhysicsWorld::AddForce(ECSWorld& world, Entity e, const glm::vec3& force) {
    auto* rb = world.GetComponent<RigidBodyComponent>(e);
    if (!rb || rb->IsStatic || rb->Mass <= 0) return;
    rb->WakeUp();
    rb->Acceleration += force / rb->Mass;
}

void PhysicsWorld::AddImpulse(ECSWorld& world, Entity e, const glm::vec3& impulse) {
    auto* rb = world.GetComponent<RigidBodyComponent>(e);
    if (!rb || rb->IsStatic || rb->Mass <= 0) return;
    rb->WakeUp();
    rb->Velocity += impulse / rb->Mass;
}

void PhysicsWorld::AddTorque(ECSWorld& world, Entity e, const glm::vec3& torque) {
    auto* rb = world.GetComponent<RigidBodyComponent>(e);
    if (!rb || rb->IsStatic || rb->Mass <= 0) return;
    rb->WakeUp();
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
