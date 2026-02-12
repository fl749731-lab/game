#pragma once

#include "engine/core/types.h"
#include <glm/glm.hpp>
#include <vector>
#include <optional>

namespace Engine {

// ── AABB 包围盒 ─────────────────────────────────────────────

struct AABB {
    glm::vec3 Min = {-0.5f, -0.5f, -0.5f};
    glm::vec3 Max = { 0.5f,  0.5f,  0.5f};

    /// 获取中心点
    glm::vec3 Center() const { return (Min + Max) * 0.5f; }

    /// 获取半尺寸
    glm::vec3 HalfSize() const { return (Max - Min) * 0.5f; }

    /// 合并另一个 AABB
    void Merge(const AABB& other) {
        Min = glm::min(Min, other.Min);
        Max = glm::max(Max, other.Max);
    }

    /// 包含点检测
    bool Contains(const glm::vec3& point) const {
        return point.x >= Min.x && point.x <= Max.x &&
               point.y >= Min.y && point.y <= Max.y &&
               point.z >= Min.z && point.z <= Max.z;
    }
};

// ── 射线 ────────────────────────────────────────────────────

struct Ray {
    glm::vec3 Origin    = {0, 0, 0};
    glm::vec3 Direction = {0, 0, -1};  // 应为单位向量

    glm::vec3 At(f32 t) const { return Origin + Direction * t; }
};

// ── 碰撞结果 ────────────────────────────────────────────────

struct HitResult {
    bool Hit = false;
    f32  Distance = 0.0f;       // 碰撞距离
    glm::vec3 Point = {0,0,0};  // 碰撞点
    glm::vec3 Normal = {0,1,0}; // 碰撞法线
};

struct CollisionPair {
    u32 EntityA = 0;
    u32 EntityB = 0;
    glm::vec3 Normal = {0,0,0};
    f32 Penetration = 0.0f;
};

// ── 碰撞检测 ────────────────────────────────────────────────

class Collision {
public:
    /// AABB vs AABB
    static bool TestAABB(const AABB& a, const AABB& b);

    /// AABB vs AABB（带穿透信息）
    static bool TestAABB(const AABB& a, const AABB& b,
                         glm::vec3& outNormal, f32& outPenetration);

    /// 射线 vs AABB
    static HitResult RaycastAABB(const Ray& ray, const AABB& aabb);

    /// 点 vs 球
    static bool TestPointSphere(const glm::vec3& point,
                                const glm::vec3& center, f32 radius);

    /// 射线 vs 平面 (y = height)
    static HitResult RaycastPlane(const Ray& ray, f32 height = 0.0f);
};

} // namespace Engine
