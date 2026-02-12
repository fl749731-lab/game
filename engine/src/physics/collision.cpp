#include "engine/physics/collision.h"

#include <cmath>
#include <algorithm>
#include <limits>

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

    // 计算穿透深度和方向（选择穿透最小的轴）
    glm::vec3 ac = a.Center(), bc = b.Center();
    glm::vec3 ahs = a.HalfSize(), bhs = b.HalfSize();
    glm::vec3 diff = bc - ac;
    glm::vec3 overlap = ahs + bhs - glm::abs(diff);

    // 最小穿透轴
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

    // 碰撞法线（哪个轴最后进入）
    if (tMin.x > tMin.y && tMin.x > tMin.z) {
        result.Normal = {invDir.x > 0 ? -1.f : 1.f, 0, 0};
    } else if (tMin.y > tMin.z) {
        result.Normal = {0, invDir.y > 0 ? -1.f : 1.f, 0};
    } else {
        result.Normal = {0, 0, invDir.z > 0 ? -1.f : 1.f};
    }

    return result;
}

// ── 点 vs 球 ────────────────────────────────────────────────

bool Collision::TestPointSphere(const glm::vec3& point,
                                const glm::vec3& center, f32 radius) {
    return glm::distance(point, center) <= radius;
}

// ── 射线 vs 平面 (y = height) ──────────────────────────────

HitResult Collision::RaycastPlane(const Ray& ray, f32 height) {
    HitResult result;
    if (fabsf(ray.Direction.y) < 1e-6f) return result; // 平行

    f32 t = (height - ray.Origin.y) / ray.Direction.y;
    if (t < 0) return result;

    result.Hit = true;
    result.Distance = t;
    result.Point = ray.At(t);
    result.Normal = {0, 1, 0};
    return result;
}

} // namespace Engine
