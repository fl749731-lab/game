#pragma once

#include "engine/core/types.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <utility>

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

// ── 球体 ───────────────────────────────────────────────────

struct Sphere {
    glm::vec3 Center = {0, 0, 0};
    f32 Radius = 0.5f;
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

    /// 球 vs 球（带穿透信息）
    static bool TestSpheres(const Sphere& a, const Sphere& b,
                            glm::vec3& outNormal, f32& outPenetration);

    /// 球 vs AABB（带穿透信息）
    static bool TestSphereAABB(const Sphere& s, const AABB& b,
                               glm::vec3& outNormal, f32& outPenetration);

    /// 射线 vs 平面 (y = height)
    static HitResult RaycastPlane(const Ray& ray, f32 height = 0.0f);

    /// 射线 vs 球
    static HitResult RaycastSphere(const Ray& ray, const Sphere& sphere);
};

// ── 空间哈希网格（宽相碰撞粗筛）────────────────────

class SpatialHash {
public:
    SpatialHash(f32 cellSize = 4.0f) : m_CellSize(cellSize) {}

    void Clear();

    /// 插入实体 AABB
    void Insert(u32 entity, const AABB& worldAABB);

    /// 获取潜在碰撞对 (去重)
    std::vector<std::pair<u32, u32>> GetPotentialPairs() const;

private:
    struct CellKey {
        int x, y, z;
        bool operator==(const CellKey& o) const { return x==o.x && y==o.y && z==o.z; }
    };
    struct CellHash {
        size_t operator()(const CellKey& k) const {
            size_t h = std::hash<int>()(k.x);
            h ^= std::hash<int>()(k.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= std::hash<int>()(k.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    CellKey ToCell(const glm::vec3& pos) const;

    f32 m_CellSize;
    std::unordered_map<CellKey, std::vector<u32>, CellHash> m_Cells;
};

} // namespace Engine
