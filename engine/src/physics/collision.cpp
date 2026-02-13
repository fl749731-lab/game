#include "engine/physics/collision.h"

#include <cmath>
#include <algorithm>
#include <limits>
#include <set>

namespace Engine {

// ── AABB vs AABB ────────────────────────────────────────────

bool Collision::TestAABB(const AABB& a, const AABB& b) {
    return (a.Min.x <= b.Max.x && a.Max.x >= b.Min.x) &&
           (a.Min.y <= b.Max.y && a.Max.y >= b.Min.y) &&
           (a.Min.z <= b.Max.z && a.Max.z >= b.Min.z);
}

bool Collision::TestAABB(const AABB& a, const AABB& b,
                         glm::vec3& outNormal, f32& outPenetration) {
    if (!TestAABB(a, b)) return false;

    glm::vec3 ac = a.Center(), bc = b.Center();
    glm::vec3 ahs = a.HalfSize(), bhs = b.HalfSize();
    glm::vec3 diff = bc - ac;
    glm::vec3 overlap = ahs + bhs - glm::abs(diff);

    if (overlap.x < overlap.y && overlap.x < overlap.z) {
        outPenetration = overlap.x;
        outNormal = {diff.x > 0 ? 1.0f : -1.0f, 0.0f, 0.0f};
    } else if (overlap.y < overlap.z) {
        outPenetration = overlap.y;
        outNormal = {0.0f, diff.y > 0 ? 1.0f : -1.0f, 0.0f};
    } else {
        outPenetration = overlap.z;
        outNormal = {0.0f, 0.0f, diff.z > 0 ? 1.0f : -1.0f};
    }
    return true;
}

// ── 球 vs 球 ────────────────────────────────────────────────

bool Collision::TestSpheres(const Sphere& a, const Sphere& b,
                            glm::vec3& outNormal, f32& outPenetration) {
    glm::vec3 diff = b.Center - a.Center;
    f32 dist = glm::length(diff);
    f32 sumR = a.Radius + b.Radius;

    if (dist >= sumR) return false;

    outPenetration = sumR - dist;
    outNormal = (dist > 0.0001f) ? diff / dist : glm::vec3(0, 1, 0);
    return true;
}

// ── 球 vs AABB ──────────────────────────────────────────────

bool Collision::TestSphereAABB(const Sphere& s, const AABB& b,
                               glm::vec3& outNormal, f32& outPenetration) {
    glm::vec3 closest = glm::clamp(s.Center, b.Min, b.Max);
    glm::vec3 diff = s.Center - closest;
    f32 dist = glm::length(diff);

    if (dist >= s.Radius) return false;

    outPenetration = s.Radius - dist;
    outNormal = (dist > 0.0001f) ? diff / dist : glm::vec3(0, 1, 0);
    return true;
}

// ── 胶囊 vs 胶囊 ────────────────────────────────────────────

bool Collision::TestCapsules(const Capsule& a, const Capsule& b,
                             glm::vec3& outNormal, f32& outPenetration) {
    // 求两线段之间的最近点对
    glm::vec3 d1 = a.PointB - a.PointA;
    glm::vec3 d2 = b.PointB - b.PointA;
    glm::vec3 r = a.PointA - b.PointA;
    f32 len1sq = glm::dot(d1, d1);
    f32 len2sq = glm::dot(d2, d2);
    f32 f = glm::dot(d2, r);

    f32 s = 0, t = 0;

    if (len1sq <= 1e-6f && len2sq <= 1e-6f) {
        // 两条退化为点
        s = t = 0;
    } else if (len1sq <= 1e-6f) {
        s = 0;
        t = glm::clamp(f / len2sq, 0.0f, 1.0f);
    } else {
        f32 c = glm::dot(d1, r);
        if (len2sq <= 1e-6f) {
            t = 0;
            s = glm::clamp(-c / len1sq, 0.0f, 1.0f);
        } else {
            f32 b_param = glm::dot(d1, d2);
            f32 denom = len1sq * len2sq - b_param * b_param;

            if (denom != 0) {
                s = glm::clamp((b_param * f - c * len2sq) / denom, 0.0f, 1.0f);
            } else {
                s = 0;
            }

            t = (b_param * s + f) / len2sq;

            if (t < 0) {
                t = 0;
                s = glm::clamp(-c / len1sq, 0.0f, 1.0f);
            } else if (t > 1) {
                t = 1;
                s = glm::clamp((b_param - c) / len1sq, 0.0f, 1.0f);
            }
        }
    }

    glm::vec3 closestA = a.PointA + d1 * s;
    glm::vec3 closestB = b.PointA + d2 * t;
    glm::vec3 diff = closestB - closestA;
    f32 dist = glm::length(diff);
    f32 sumR = a.Radius + b.Radius;

    if (dist >= sumR) return false;

    outPenetration = sumR - dist;
    outNormal = (dist > 0.0001f) ? diff / dist : glm::vec3(0, 1, 0);
    return true;
}

// ── 胶囊 vs AABB ────────────────────────────────────────────

bool Collision::TestCapsuleAABB(const Capsule& cap, const AABB& aabb,
                                glm::vec3& outNormal, f32& outPenetration) {
    // 找到胶囊线段上离 AABB 最近的点，然后做球 vs AABB
    glm::vec3 aabbCenter = aabb.Center();
    glm::vec3 closestOnSeg = Capsule::ClosestPointOnSegment(aabbCenter, cap.PointA, cap.PointB);

    Sphere sph;
    sph.Center = closestOnSeg;
    sph.Radius = cap.Radius;
    return TestSphereAABB(sph, aabb, outNormal, outPenetration);
}

// ── 胶囊 vs 球 ──────────────────────────────────────────────

bool Collision::TestCapsuleSphere(const Capsule& cap, const Sphere& sph,
                                  glm::vec3& outNormal, f32& outPenetration) {
    glm::vec3 closestOnSeg = Capsule::ClosestPointOnSegment(sph.Center, cap.PointA, cap.PointB);

    glm::vec3 diff = sph.Center - closestOnSeg;
    f32 dist = glm::length(diff);
    f32 sumR = cap.Radius + sph.Radius;

    if (dist >= sumR) return false;

    outPenetration = sumR - dist;
    outNormal = (dist > 0.0001f) ? diff / dist : glm::vec3(0, 1, 0);
    return true;
}

// ── 射线 vs AABB (Slab method) ─────────────────────────────

HitResult Collision::RaycastAABB(const Ray& ray, const AABB& aabb) {
    HitResult result;

    glm::vec3 invDir = 1.0f / ray.Direction;
    glm::vec3 t1 = (aabb.Min - ray.Origin) * invDir;
    glm::vec3 t2 = (aabb.Max - ray.Origin) * invDir;

    glm::vec3 tMin = glm::min(t1, t2);
    glm::vec3 tMax = glm::max(t1, t2);

    f32 tNear = std::max({tMin.x, tMin.y, tMin.z});
    f32 tFar  = std::min({tMax.x, tMax.y, tMax.z});

    if (tNear > tFar || tFar < 0.0f) return result;

    result.Hit = true;
    result.Distance = tNear > 0 ? tNear : tFar;
    result.Point = ray.At(result.Distance);

    if (tMin.x > tMin.y && tMin.x > tMin.z) {
        result.Normal = {invDir.x > 0 ? -1.f : 1.f, 0, 0};
    } else if (tMin.y > tMin.z) {
        result.Normal = {0, invDir.y > 0 ? -1.f : 1.f, 0};
    } else {
        result.Normal = {0, 0, invDir.z > 0 ? -1.f : 1.f};
    }

    return result;
}

// ── 射线 vs 球 ──────────────────────────────────────────────

HitResult Collision::RaycastSphere(const Ray& ray, const Sphere& sphere) {
    HitResult result;
    glm::vec3 oc = ray.Origin - sphere.Center;
    f32 a = glm::dot(ray.Direction, ray.Direction);
    f32 b = 2.0f * glm::dot(oc, ray.Direction);
    f32 c = glm::dot(oc, oc) - sphere.Radius * sphere.Radius;
    f32 disc = b * b - 4 * a * c;

    if (disc < 0) return result;

    f32 t = (-b - sqrtf(disc)) / (2.0f * a);
    if (t < 0) t = (-b + sqrtf(disc)) / (2.0f * a);
    if (t < 0) return result;

    result.Hit = true;
    result.Distance = t;
    result.Point = ray.At(t);
    result.Normal = glm::normalize(result.Point - sphere.Center);
    return result;
}

// ── 射线 vs 胶囊 ────────────────────────────────────────────

HitResult Collision::RaycastCapsule(const Ray& ray, const Capsule& capsule) {
    HitResult result;

    // 先对胶囊线段做无限圆柱检测，然后检查是否在线段范围内
    // 如果不在范围内，检测端点半球
    glm::vec3 d = capsule.PointB - capsule.PointA;
    f32 segLen = glm::length(d);
    if (segLen < 1e-6f) {
        // 退化为球
        Sphere sph = {capsule.PointA, capsule.Radius};
        return RaycastSphere(ray, sph);
    }

    // 先用 AABB 粗筛
    AABB capsuleAABB = capsule.ToAABB();
    auto aabbHit = RaycastAABB(ray, capsuleAABB);
    if (!aabbHit.Hit) return result;

    // 检测两端球
    Sphere sphereA = {capsule.PointA, capsule.Radius};
    Sphere sphereB = {capsule.PointB, capsule.Radius};
    auto hitA = RaycastSphere(ray, sphereA);
    auto hitB = RaycastSphere(ray, sphereB);

    if (hitA.Hit && (!result.Hit || hitA.Distance < result.Distance)) result = hitA;
    if (hitB.Hit && (!result.Hit || hitB.Distance < result.Distance)) result = hitB;

    // 检测圆柱体部分（简化：用膨胀的线段检测）
    // 沿轴方向投影射线检测
    glm::vec3 axis = d / segLen;
    glm::vec3 oc = ray.Origin - capsule.PointA;
    glm::vec3 dirPerp = ray.Direction - axis * glm::dot(ray.Direction, axis);
    glm::vec3 ocPerp = oc - axis * glm::dot(oc, axis);

    f32 a = glm::dot(dirPerp, dirPerp);
    f32 b = 2.0f * glm::dot(dirPerp, ocPerp);
    f32 c = glm::dot(ocPerp, ocPerp) - capsule.Radius * capsule.Radius;
    f32 disc = b * b - 4 * a * c;

    if (disc >= 0 && a > 1e-6f) {
        f32 t = (-b - sqrtf(disc)) / (2.0f * a);
        if (t < 0) t = (-b + sqrtf(disc)) / (2.0f * a);
        if (t >= 0) {
            glm::vec3 hitPt = ray.At(t);
            f32 proj = glm::dot(hitPt - capsule.PointA, axis);
            if (proj >= 0 && proj <= segLen) {
                if (!result.Hit || t < result.Distance) {
                    result.Hit = true;
                    result.Distance = t;
                    result.Point = hitPt;
                    glm::vec3 axisPoint = capsule.PointA + axis * proj;
                    result.Normal = glm::normalize(hitPt - axisPoint);
                }
            }
        }
    }

    return result;
}

// ── 点 vs 球 ────────────────────────────────────────────────

bool Collision::TestPointSphere(const glm::vec3& point,
                                const glm::vec3& center, f32 radius) {
    return glm::distance(point, center) <= radius;
}

// ── 射线 vs 平面 ────────────────────────────────────────────

HitResult Collision::RaycastPlane(const Ray& ray, f32 height) {
    HitResult result;
    if (fabsf(ray.Direction.y) < 1e-6f) return result;

    f32 t = (height - ray.Origin.y) / ray.Direction.y;
    if (t < 0) return result;

    result.Hit = true;
    result.Distance = t;
    result.Point = ray.At(t);
    result.Normal = {0, 1, 0};
    return result;
}

// ── 空间哈希网格 ────────────────────────────────────────────

SpatialHash::CellKey SpatialHash::ToCell(const glm::vec3& pos) const {
    return {
        (int)floorf(pos.x / m_CellSize),
        (int)floorf(pos.y / m_CellSize),
        (int)floorf(pos.z / m_CellSize)
    };
}

void SpatialHash::Clear() {
    m_Cells.clear();
}

void SpatialHash::Insert(u32 entity, const AABB& worldAABB) {
    CellKey minCell = ToCell(worldAABB.Min);
    CellKey maxCell = ToCell(worldAABB.Max);

    for (int x = minCell.x; x <= maxCell.x; x++) {
        for (int y = minCell.y; y <= maxCell.y; y++) {
            for (int z = minCell.z; z <= maxCell.z; z++) {
                m_Cells[{x, y, z}].push_back(entity);
            }
        }
    }
}

std::vector<std::pair<u32, u32>> SpatialHash::GetPotentialPairs() const {
    std::set<std::pair<u32, u32>> unique;

    for (auto& [key, entities] : m_Cells) {
        for (size_t i = 0; i < entities.size(); i++) {
            for (size_t j = i + 1; j < entities.size(); j++) {
                u32 a = std::min(entities[i], entities[j]);
                u32 b = std::max(entities[i], entities[j]);
                unique.insert({a, b});
            }
        }
    }

    return {unique.begin(), unique.end()};
}

} // namespace Engine
