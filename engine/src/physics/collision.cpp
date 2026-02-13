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
    // 找到 AABB 上距离球心最近的点
    glm::vec3 closest = glm::clamp(s.Center, b.Min, b.Max);
    glm::vec3 diff = s.Center - closest;
    f32 dist = glm::length(diff);

    if (dist >= s.Radius) return false;

    outPenetration = s.Radius - dist;
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

