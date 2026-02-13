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
std::vector<u32> PhysicsWorld::s_FreeSlots;
f32 PhysicsWorld::s_Accumulator = 0.0f;
PhysicsConfig PhysicsWorld::s_Config;
std::unordered_set<PhysicsWorld::PairKey, PhysicsWorld::PairHash> PhysicsWorld::s_PreviousPairs;
std::unordered_set<PhysicsWorld::PairKey, PhysicsWorld::PairHash> PhysicsWorld::s_CurrentPairs;
std::vector<CollisionEventData> PhysicsWorld::s_CollisionEvents;
std::mutex PhysicsWorld::s_Mutex;

// ── 配置 ────────────────────────────────────────────────────

void PhysicsWorld::SetConfig(const PhysicsConfig& cfg) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    s_Config = cfg;
}

const PhysicsConfig& PhysicsWorld::GetConfig() { return s_Config; }

// ── 固定步长累积器 ──────────────────────────────────────────

void PhysicsWorld::Update(ECSWorld& world, f32 frameTime) {
    std::lock_guard<std::mutex> lock(s_Mutex);

    frameTime = std::min(frameTime, s_Config.MaxAccumulator);
    s_Accumulator += frameTime;

    while (s_Accumulator >= s_Config.FixedTimestep) {
        Step(world, s_Config.FixedTimestep);
        s_Accumulator -= s_Config.FixedTimestep;
    }
}

// ── 主更新 ──────────────────────────────────────────────────

void PhysicsWorld::Step(ECSWorld& world, f32 dt) {
    UpdateSleep(world, dt);
    IntegrateForces(world, dt);
    PerformCCD(world, dt);
    DetectCollisions(world);
    UpdateCollisionEvents();

    // 多次速度迭代提高稳定性
    for (i32 v = 0; v < s_Config.VelocityIters; v++) {
        ResolveCollisions(world);
    }

    SolveConstraints(world, dt);
    ResolveGroundCollisions(world);
    UpdateCharacterControllers(world, dt);
}

// ── 速度钳制 ────────────────────────────────────────────────

void PhysicsWorld::ClampVelocities(RigidBodyComponent& rb) {
    // 线性速度上限
    f32 linSpeed = glm::length(rb.Velocity);
    if (linSpeed > s_Config.MaxVelocity) {
        rb.Velocity = (rb.Velocity / linSpeed) * s_Config.MaxVelocity;
    }
    // 角速度上限
    f32 angSpeed = glm::length(rb.AngularVelocity);
    if (angSpeed > s_Config.MaxAngularVel) {
        rb.AngularVelocity = (rb.AngularVelocity / angSpeed) * s_Config.MaxAngularVel;
    }
    // NaN/Inf 保护
    if (std::isnan(rb.Velocity.x) || std::isinf(rb.Velocity.x)) rb.Velocity = {0,0,0};
    if (std::isnan(rb.AngularVelocity.x) || std::isinf(rb.AngularVelocity.x)) rb.AngularVelocity = {0,0,0};
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

        if (linearSpeed < s_Config.SleepLinear && angularSpeed < s_Config.SleepAngular) {
            rb.SleepTimer += dt;
            if (rb.SleepTimer >= s_Config.SleepDelay) {
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

        // 速度钳制（防止数值爆炸）
        ClampVelocities(rb);

        // 半隐式 Euler
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

// ── 复合碰撞体窄相 ─────────────────────────────────────────

bool PhysicsWorld::TestSingleShape(ColliderShape shapeA, const ColliderComponent& colA,
                                    const TransformComponent& trA, const glm::vec3& offsetA,
                                    ColliderShape shapeB, const ColliderComponent& colB,
                                    const TransformComponent& trB, const glm::vec3& offsetB,
                                    glm::vec3& outNormal, f32& outPenetration) {
    // 创建偏移后的 transform
    TransformComponent trAOff = trA;
    trAOff.X += offsetA.x * trA.ScaleX;
    trAOff.Y += offsetA.y * trA.ScaleY;
    trAOff.Z += offsetA.z * trA.ScaleZ;

    TransformComponent trBOff = trB;
    trBOff.X += offsetB.x * trB.ScaleX;
    trBOff.Y += offsetB.y * trB.ScaleY;
    trBOff.Z += offsetB.z * trB.ScaleZ;

    // Box vs Box
    if (shapeA == ColliderShape::Box && shapeB == ColliderShape::Box) {
        AABB a = colA.GetWorldAABB(trAOff);
        AABB b = colB.GetWorldAABB(trBOff);
        return Collision::TestAABB(a, b, outNormal, outPenetration);
    }

    // Sphere vs Sphere
    if (shapeA == ColliderShape::Sphere && shapeB == ColliderShape::Sphere) {
        return Collision::TestSpheres(colA.GetWorldSphere(trAOff),
                                      colB.GetWorldSphere(trBOff),
                                      outNormal, outPenetration);
    }

    // Sphere vs Box
    if (shapeA == ColliderShape::Sphere && shapeB == ColliderShape::Box) {
        return Collision::TestSphereAABB(colA.GetWorldSphere(trAOff),
                                         colB.GetWorldAABB(trBOff),
                                         outNormal, outPenetration);
    }
    if (shapeA == ColliderShape::Box && shapeB == ColliderShape::Sphere) {
        bool hit = Collision::TestSphereAABB(colB.GetWorldSphere(trBOff),
                                              colA.GetWorldAABB(trAOff),
                                              outNormal, outPenetration);
        outNormal = -outNormal;
        return hit;
    }

    // Capsule vs Capsule
    if (shapeA == ColliderShape::Capsule && shapeB == ColliderShape::Capsule) {
        return Collision::TestCapsules(colA.GetWorldCapsule(trAOff),
                                       colB.GetWorldCapsule(trBOff),
                                       outNormal, outPenetration);
    }

    // Capsule vs Box
    if (shapeA == ColliderShape::Capsule && shapeB == ColliderShape::Box) {
        return Collision::TestCapsuleAABB(colA.GetWorldCapsule(trAOff),
                                          colB.GetWorldAABB(trBOff),
                                          outNormal, outPenetration);
    }
    if (shapeA == ColliderShape::Box && shapeB == ColliderShape::Capsule) {
        bool hit = Collision::TestCapsuleAABB(colB.GetWorldCapsule(trBOff),
                                               colA.GetWorldAABB(trAOff),
                                               outNormal, outPenetration);
        outNormal = -outNormal;
        return hit;
    }

    // Capsule vs Sphere
    if (shapeA == ColliderShape::Capsule && shapeB == ColliderShape::Sphere) {
        return Collision::TestCapsuleSphere(colA.GetWorldCapsule(trAOff),
                                            colB.GetWorldSphere(trBOff),
                                            outNormal, outPenetration);
    }
    if (shapeA == ColliderShape::Sphere && shapeB == ColliderShape::Capsule) {
        bool hit = Collision::TestCapsuleSphere(colB.GetWorldCapsule(trBOff),
                                                colA.GetWorldSphere(trAOff),
                                                outNormal, outPenetration);
        outNormal = -outNormal;
        return hit;
    }

    // 后备
    AABB a = colA.GetWorldAABB(trAOff);
    AABB b = colB.GetWorldAABB(trBOff);
    return Collision::TestAABB(a, b, outNormal, outPenetration);
}

bool PhysicsWorld::TestColliders(const ColliderComponent& colA, const TransformComponent& trA,
                                  const ColliderComponent& colB, const TransformComponent& trB,
                                  glm::vec3& outNormal, f32& outPenetration) {
    // 碰撞层过滤
    if (!Collision::LayersCanCollide(colA.Layer, colA.Mask, colB.Layer, colB.Mask))
        return false;

    bool hasSubA = !colA.SubShapes.empty();
    bool hasSubB = !colB.SubShapes.empty();

    // 无复合碰撞体 — 直接测试主形状
    if (!hasSubA && !hasSubB) {
        return TestSingleShape(colA.Shape, colA, trA, {0,0,0},
                               colB.Shape, colB, trB, {0,0,0},
                               outNormal, outPenetration);
    }

    // 复合碰撞体 — 遍历所有子形状对，找最深穿透
    f32 maxPen = 0.0f;
    bool anyHit = false;

    auto testPair = [&](ColliderShape sA, const glm::vec3& offA,
                        ColliderShape sB, const glm::vec3& offB) {
        glm::vec3 n;
        f32 pen;
        if (TestSingleShape(sA, colA, trA, offA, sB, colB, trB, offB, n, pen)) {
            if (pen > maxPen) {
                maxPen = pen;
                outNormal = n;
                outPenetration = pen;
            }
            anyHit = true;
        }
    };

    // 列出 A 的所有形状
    struct ShapeEntry { ColliderShape shape; glm::vec3 offset; };
    std::vector<ShapeEntry> shapesA, shapesB;

    if (hasSubA) {
        for (auto& sub : colA.SubShapes)
            shapesA.push_back({sub.Shape, sub.Offset});
    } else {
        shapesA.push_back({colA.Shape, {0,0,0}});
    }

    if (hasSubB) {
        for (auto& sub : colB.SubShapes)
            shapesB.push_back({sub.Shape, sub.Offset});
    } else {
        shapesB.push_back({colB.Shape, {0,0,0}});
    }

    for (auto& a : shapesA) {
        for (auto& b : shapesB) {
            testPair(a.shape, a.offset, b.shape, b.offset);
        }
    }

    return anyHit;
}

// ── CCD ─────────────────────────────────────────────────────

CCDResult PhysicsWorld::SweepTest(ECSWorld& world, Entity e,
                                   const glm::vec3& displacement) {
    CCDResult result;
    auto* col = world.GetComponent<ColliderComponent>(e);
    auto* tr = world.GetComponent<TransformComponent>(e);
    if (!col || !tr) return result;

    f32 dispLen = glm::length(displacement);
    if (dispLen < 1e-6f) return result;

    glm::vec3 dir = displacement / dispLen;

    // 根据碰撞体形状选择扫掠策略
    AABB startAABB = col->GetWorldAABB(*tr);
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
        if (!Collision::LayersCanCollide(col->Layer, col->Mask, otherCol.Layer, otherCol.Mask))
            continue;

        auto* otherTr = world.GetComponent<TransformComponent>(other);
        if (!otherTr) continue;

        AABB otherAABB = otherCol.GetWorldAABB(*otherTr);
        if (!Collision::TestAABB(sweepAABB, otherAABB)) continue;

        // 精确 TOI：根据源碰撞体形状选择扫掠方式
        f32 toi = 1.0f;
        glm::vec3 hitNormal = {0,1,0};
        bool hit = false;

        if (col->Shape == ColliderShape::Sphere) {
            // 球体扫掠：膨胀目标 AABB
            Sphere sph = col->GetWorldSphere(*tr);
            AABB expanded;
            expanded.Min = otherAABB.Min - glm::vec3(sph.Radius);
            expanded.Max = otherAABB.Max + glm::vec3(sph.Radius);

            Ray sweepRay;
            sweepRay.Origin = sph.Center;
            sweepRay.Direction = dir;
            HitResult h = Collision::RaycastAABB(sweepRay, expanded);
            if (h.Hit && h.Distance >= 0 && h.Distance <= dispLen) {
                toi = h.Distance / dispLen;
                hitNormal = h.Normal;
                hit = true;
            }
        } else {
            // AABB/Capsule: Minkowski AABB 扫掠
            glm::vec3 halfA = startAABB.HalfSize();
            AABB minkowski;
            minkowski.Min = otherAABB.Min - halfA;
            minkowski.Max = otherAABB.Max + halfA;

            Ray sweepRay;
            sweepRay.Origin = startAABB.Center();
            sweepRay.Direction = dir;
            HitResult h = Collision::RaycastAABB(sweepRay, minkowski);
            if (h.Hit && h.Distance >= 0 && h.Distance <= dispLen) {
                toi = h.Distance / dispLen;
                hitNormal = h.Normal;
                hit = true;
            }
        }

        if (hit && toi < minTOI) {
            minTOI = toi;
            result.Hit = true;
            result.TOI = toi;
            result.HitNormal = hitNormal;
            result.HitPoint = glm::vec3(tr->X, tr->Y, tr->Z) + displacement * toi;
            result.HitEntity = other;
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
        if (minHalf < 1e-6f) minHalf = 0.1f; // 保护零尺寸

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
            ClampVelocities(rb);
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

            if (s_Callback) s_Callback(pair.EntityA, pair.EntityB, pair.Normal);

            CollisionEvent evt(pair.EntityA, pair.EntityB,
                               normal.x, normal.y, normal.z, penetration);
            EventBus::Dispatch(evt);
        }
    };

    if (entities.size() <= 32) {
        for (size_t i = 0; i < entities.size(); i++) {
            for (size_t j = i + 1; j < entities.size(); j++) {
                if (Collision::TestAABB(entities[i].worldAABB, entities[j].worldAABB))
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
            if (itA != entityIndex.end() && itB != entityIndex.end())
                narrowPhase(itA->second, itB->second);
        }
    }
}

// ── 碰撞事件 ────────────────────────────────────────────────

void PhysicsWorld::UpdateCollisionEvents() {
    s_CollisionEvents.clear();

    for (auto& pair : s_CurrentPairs) {
        if (s_PreviousPairs.find(pair) == s_PreviousPairs.end())
            s_CollisionEvents.push_back({pair.a, pair.b, CollisionState::Enter});
        else
            s_CollisionEvents.push_back({pair.a, pair.b, CollisionState::Stay});
    }

    for (auto& pair : s_PreviousPairs) {
        if (s_CurrentPairs.find(pair) == s_CurrentPairs.end())
            s_CollisionEvents.push_back({pair.a, pair.b, CollisionState::Exit});
    }

    if (s_EventCallback) {
        for (auto& evt : s_CollisionEvents) s_EventCallback(evt);
    }

    s_PreviousPairs = s_CurrentPairs;
}

// ── 碰撞响应（含数值稳定性保护）─────────────────────────

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

        if (rbA && rbA->IsSleeping) rbA->WakeUp();
        if (rbB && rbB->IsSleeping) rbB->WakeUp();

        // 安全计算逆质量（有质量比上限保护）
        f32 invMassA = staticA ? 0.0f : rbA->InvMass();
        f32 invMassB = staticB ? 0.0f : rbB->InvMass();

        // 质量比钳制（防止极端质量比导致抖动）
        if (invMassA > 0 && invMassB > 0) {
            f32 ratio = std::max(invMassA, invMassB) / std::min(invMassA, invMassB);
            if (ratio > s_Config.MaxMassRatio) {
                f32 scaleFactor = s_Config.MaxMassRatio / ratio;
                if (invMassA > invMassB) invMassA *= scaleFactor;
                else invMassB *= scaleFactor;
            }
        }

        f32 totalInvMass = invMassA + invMassB;
        if (totalInvMass < 1e-8f) continue;

        // 材质混合
        f32 restitution = std::min(colA->Material.Restitution, colB->Material.Restitution);
        f32 friction = std::sqrt(colA->Material.Friction * colB->Material.Friction);

        // 位置分离（含穿透容差 slop + Baumgarte 偏差）
        f32 correctionMag = std::max(pair.Penetration - s_Config.PenetrationSlop, 0.0f)
                          * s_Config.BaumgarteBias / totalInvMass;
        glm::vec3 correction = pair.Normal * correctionMag;

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

        // 法向冲量
        glm::vec3 velA = staticA ? glm::vec3(0) : rbA->Velocity;
        glm::vec3 velB = staticB ? glm::vec3(0) : rbB->Velocity;
        glm::vec3 relVel = velA - velB;
        f32 velAlongNormal = glm::dot(relVel, pair.Normal);
        if (velAlongNormal > 0) continue;

        f32 j = -(1.0f + restitution) * velAlongNormal / totalInvMass;
        glm::vec3 impulse = pair.Normal * j;
        if (rbA && !rbA->IsStatic) rbA->Velocity += impulse * invMassA;
        if (rbB && !rbB->IsStatic) rbB->Velocity -= impulse * invMassB;

        // 摩擦冲量
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

        // 钳制结果速度
        if (rbA && !rbA->IsStatic) ClampVelocities(*rbA);
        if (rbB && !rbB->IsStatic) ClampVelocities(*rbB);
    }
}

// ── 约束求解 ────────────────────────────────────────────────

void PhysicsWorld::SolveConstraints(ECSWorld& world, f32 dt) {
    for (i32 iter = 0; iter < s_Config.ConstraintIters; iter++) {
        for (auto& c : s_Constraints) {
            if (!c.Active_ || !c.Enabled) continue;
            if (c.EntityA == INVALID_ENTITY || c.EntityB == INVALID_ENTITY) continue;

            auto* trA = world.GetComponent<TransformComponent>(c.EntityA);
            auto* trB = world.GetComponent<TransformComponent>(c.EntityB);
            if (!trA || !trB) continue;

            auto* rbA = world.GetComponent<RigidBodyComponent>(c.EntityA);
            auto* rbB = world.GetComponent<RigidBodyComponent>(c.EntityB);

            bool staticA = (!rbA || rbA->IsStatic);
            bool staticB = (!rbB || rbB->IsStatic);
            if (staticA && staticB) continue;

            f32 invMassA = staticA ? 0.0f : rbA->InvMass();
            f32 invMassB = staticB ? 0.0f : rbB->InvMass();
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
                glm::vec3 correction = dir * error * s_Config.BaumgarteBias / totalInvMass;

                if (!staticA) { trA->X += correction.x * invMassA; trA->Y += correction.y * invMassA; trA->Z += correction.z * invMassA; }
                if (!staticB) { trB->X -= correction.x * invMassB; trB->Y -= correction.y * invMassB; trB->Z -= correction.z * invMassB; }

                if (rbA || rbB) {
                    glm::vec3 relVel = (staticA ? glm::vec3(0) : rbA->Velocity) -
                                       (staticB ? glm::vec3(0) : rbB->Velocity);
                    f32 velAlongDir = glm::dot(relVel, dir);
                    glm::vec3 velCorr = dir * velAlongDir / totalInvMass;
                    if (rbA && !rbA->IsStatic) rbA->Velocity -= velCorr * invMassA;
                    if (rbB && !rbB->IsStatic) rbB->Velocity += velCorr * invMassB;
                }
                break;
            }

            case ConstraintType::Spring: {
                glm::vec3 delta = worldAnchorB - worldAnchorA;
                f32 currentDist = glm::length(delta);
                if (currentDist < 1e-6f) break;

                glm::vec3 dir = delta / currentDist;
                f32 stretch = currentDist - c.Distance;
                glm::vec3 relVel = (staticA ? glm::vec3(0) : rbA->Velocity) -
                                   (staticB ? glm::vec3(0) : rbB->Velocity);
                f32 velAlongDir = glm::dot(relVel, dir);
                f32 forceMag = c.Stiffness * stretch + c.Damping * velAlongDir;
                glm::vec3 force = dir * forceMag;

                if (rbA && !rbA->IsStatic) { rbA->Velocity += force * (invMassA * dt); ClampVelocities(*rbA); }
                if (rbB && !rbB->IsStatic) { rbB->Velocity -= force * (invMassB * dt); ClampVelocities(*rbB); }
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
                    rbA->AngularVelocity = axis * glm::dot(rbA->AngularVelocity, axis);
                }
                if (rbB && !rbB->IsStatic) {
                    rbB->AngularVelocity = axis * glm::dot(rbB->AngularVelocity, axis);
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

// ── 角色控制器（扫掠检测替代射线）──────────────────────────

void PhysicsWorld::UpdateCharacterControllers(ECSWorld& world, f32 dt) {
    auto& ccPool = world.GetComponentArray<CharacterControllerComponent>();
    u32 count = ccPool.Size();

    for (u32 i = 0; i < count; i++) {
        CharacterControllerComponent& cc = ccPool.Data(i);
        Entity e = ccPool.GetEntity(i);
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) continue;

        f32 halfH = (cc.Height - 2.0f * cc.Radius) * 0.5f;
        f32 feetY = tr->Y - halfH - cc.Radius;

        cc.IsGrounded = (feetY <= s_GroundHeight + 0.05f);

        // 重力 + 跳跃
        if (cc.IsGrounded) {
            if (cc.VerticalSpeed < 0) cc.VerticalSpeed = 0;
            if (cc.WantsJump) {
                cc.VerticalSpeed = cc.JumpForce;
                cc.IsGrounded = false;
            }
        } else {
            cc.VerticalSpeed += cc.Gravity * dt;
        }

        // 水平移动 — 用扫掠检测碰撞
        glm::vec3 moveDir = cc.MoveDir;
        f32 moveLen = glm::length(moveDir);
        if (moveLen > 1e-4f) {
            moveDir /= moveLen;
            if (moveLen > 1.0f) moveLen = 1.0f;

            glm::vec3 displacement = glm::vec3(moveDir.x, 0, moveDir.z)
                                   * cc.MoveSpeed * moveLen * dt;

            // 构建临时胶囊碰撞体进行扫掠
            auto* col = world.GetComponent<ColliderComponent>(e);
            if (col) {
                CCDResult sweep = SweepTest(world, e, displacement);
                if (sweep.Hit && sweep.TOI < 1.0f) {
                    // 滑动：去掉法线分量继续运动
                    f32 safeT = std::max(0.0f, sweep.TOI - 0.01f);
                    glm::vec3 safeDist = displacement * safeT;
                    tr->X += safeDist.x;
                    tr->Z += safeDist.z;

                    // 滑动分量
                    glm::vec3 remaining = displacement * (1.0f - safeT);
                    glm::vec3 slideDir = remaining - sweep.HitNormal * glm::dot(remaining, sweep.HitNormal);
                    tr->X += slideDir.x;
                    tr->Z += slideDir.z;

                    // 台阶检测
                    if (cc.IsGrounded && std::abs(sweep.HitNormal.y) < 0.3f) {
                        // 水平碰撞 → 检查是否可以踏上
                        glm::vec3 stepUp = {0, cc.StepHeight, 0};
                        TransformComponent trStep = *tr;
                        trStep.Y += cc.StepHeight;
                        // 简化：直接抬升检测
                        f32 slopeAngle = glm::degrees(std::acos(
                            glm::clamp(std::abs(sweep.HitNormal.y), 0.0f, 1.0f)));
                        if (slopeAngle > 90.0f - cc.SlopeLimit) {
                            tr->Y += cc.StepHeight;
                        }
                    }
                } else {
                    tr->X += displacement.x;
                    tr->Z += displacement.z;
                }
            } else {
                tr->X += displacement.x;
                tr->Z += displacement.z;
            }
        }

        // 垂直
        tr->Y += cc.VerticalSpeed * dt;

        // 地面钳制
        feetY = tr->Y - halfH - cc.Radius;
        if (feetY < s_GroundHeight) {
            tr->Y += s_GroundHeight - feetY;
            cc.VerticalSpeed = 0;
            cc.IsGrounded = true;
        }

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

void PhysicsWorld::SetCollisionCallback(CollisionCallback cb) { std::lock_guard<std::mutex> lock(s_Mutex); s_Callback = cb; }
void PhysicsWorld::SetCollisionEventCallback(CollisionEventCallback cb) { std::lock_guard<std::mutex> lock(s_Mutex); s_EventCallback = cb; }
const std::vector<CollisionPair>& PhysicsWorld::GetCollisionPairs() { return s_Pairs; }
const std::vector<CollisionEventData>& PhysicsWorld::GetCollisionEvents() { return s_CollisionEvents; }
void PhysicsWorld::SetGroundPlane(f32 height) { s_GroundHeight = height; }
f32  PhysicsWorld::GetGroundPlane() { return s_GroundHeight; }

// ── 力 / 冲量 / 力矩（线程安全 + 唤醒 + 钳制）──────────────

void PhysicsWorld::AddForce(ECSWorld& world, Entity e, const glm::vec3& force) {
    auto* rb = world.GetComponent<RigidBodyComponent>(e);
    if (!rb || rb->IsStatic || rb->Mass <= 1e-6f) return;
    rb->WakeUp();
    rb->Acceleration += force / rb->Mass;
}

void PhysicsWorld::AddImpulse(ECSWorld& world, Entity e, const glm::vec3& impulse) {
    auto* rb = world.GetComponent<RigidBodyComponent>(e);
    if (!rb || rb->IsStatic || rb->Mass <= 1e-6f) return;
    rb->WakeUp();
    rb->Velocity += impulse / rb->Mass;
    ClampVelocities(*rb);
}

void PhysicsWorld::AddTorque(ECSWorld& world, Entity e, const glm::vec3& torque) {
    auto* rb = world.GetComponent<RigidBodyComponent>(e);
    if (!rb || rb->IsStatic || rb->Mass <= 1e-6f) return;
    rb->WakeUp();
    rb->AngularVelocity += torque / rb->Mass;
    ClampVelocities(*rb);
}

// ── 约束管理（Generation-Based 槽位复用）────────────────────

ConstraintHandle PhysicsWorld::AddConstraint(const Constraint& c) {
    std::lock_guard<std::mutex> lock(s_Mutex);

    ConstraintHandle handle;

    if (!s_FreeSlots.empty()) {
        // 复用空闲槽位
        u32 idx = s_FreeSlots.back();
        s_FreeSlots.pop_back();
        s_Constraints[idx] = c;
        s_Constraints[idx].Generation_++;
        s_Constraints[idx].Active_ = true;
        handle.Index = idx;
        handle.Generation = s_Constraints[idx].Generation_;
    } else {
        // 新建槽位
        Constraint newC = c;
        newC.Generation_ = 1;
        newC.Active_ = true;
        handle.Index = (u32)s_Constraints.size();
        handle.Generation = 1;
        s_Constraints.push_back(newC);
    }

    return handle;
}

void PhysicsWorld::RemoveConstraint(ConstraintHandle handle) {
    std::lock_guard<std::mutex> lock(s_Mutex);

    if (handle.Index >= s_Constraints.size()) return;
    auto& c = s_Constraints[handle.Index];
    if (!c.Active_ || c.Generation_ != handle.Generation) return; // 已失效

    c.Active_ = false;
    c.Enabled = false;
    s_FreeSlots.push_back(handle.Index);
}

Constraint* PhysicsWorld::GetConstraint(ConstraintHandle handle) {
    if (handle.Index >= s_Constraints.size()) return nullptr;
    auto& c = s_Constraints[handle.Index];
    if (!c.Active_ || c.Generation_ != handle.Generation) return nullptr;
    return &c;
}

void PhysicsWorld::ClearConstraints() {
    std::lock_guard<std::mutex> lock(s_Mutex);
    s_Constraints.clear();
    s_FreeSlots.clear();
}

u32 PhysicsWorld::GetConstraintCount() {
    u32 count = 0;
    for (auto& c : s_Constraints) {
        if (c.Active_) count++;
    }
    return count;
}

} // namespace Engine
