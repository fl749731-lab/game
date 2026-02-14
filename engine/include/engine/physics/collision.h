#pragma once

#include "engine/core/types.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <utility>
#include <unordered_set>

namespace Engine {

// ── 碰撞层 ──────────────────────────────────────────────────

namespace CollisionLayer {
    constexpr u16 Default    = 1 << 0;   // 默认层
    constexpr u16 Static     = 1 << 1;   // 静态环境
    constexpr u16 Player     = 1 << 2;   // 玩家
    constexpr u16 Enemy      = 1 << 3;   // 敌人
    constexpr u16 Projectile = 1 << 4;   // 子弹/抛射物
    constexpr u16 Trigger    = 1 << 5;   // 触发区域
    constexpr u16 Pickup     = 1 << 6;   // 拾取物
    constexpr u16 Terrain    = 1 << 7;   // 地形
    constexpr u16 All        = 0xFFFF;
}

// ── 物理材质 ────────────────────────────────────────────────

struct PhysicsMaterial {
    f32 Restitution = 0.3f;   // 弹性 0~1
    f32 Friction    = 0.5f;   // 摩擦
    f32 Density     = 1.0f;   // 密度 (kg/m³)

    static PhysicsMaterial Default()  { return {0.3f, 0.5f, 1.0f}; }
    static PhysicsMaterial Bouncy()   { return {0.9f, 0.2f, 1.0f}; }
    static PhysicsMaterial Ice()      { return {0.1f, 0.05f, 0.9f}; }
    static PhysicsMaterial Rubber()   { return {0.8f, 0.8f, 1.1f}; }
    static PhysicsMaterial Metal()    { return {0.2f, 0.4f, 7.8f}; }
    static PhysicsMaterial Wood()     { return {0.4f, 0.6f, 0.6f}; }
};

// ── AABB 包围盒 ─────────────────────────────────────────────

struct AABB {
    glm::vec3 Min = {-0.5f, -0.5f, -0.5f};
    glm::vec3 Max = { 0.5f,  0.5f,  0.5f};

    glm::vec3 Center() const { return (Min + Max) * 0.5f; }
    glm::vec3 HalfSize() const { return (Max - Min) * 0.5f; }
    glm::vec3 Size() const { return Max - Min; }
    glm::vec3 Extents() const { return HalfSize(); }

    float SurfaceArea() const {
        glm::vec3 d = Size();
        return 2.0f * (d.x*d.y + d.y*d.z + d.z*d.x);
    }

    void Expand(const glm::vec3& point) {
        Min = glm::min(Min, point);
        Max = glm::max(Max, point);
    }

    void Expand(const AABB& other) {
        Min = glm::min(Min, other.Min);
        Max = glm::max(Max, other.Max);
    }

    void Merge(const AABB& other) {
        Min = glm::min(Min, other.Min);
        Max = glm::max(Max, other.Max);
    }

    bool Contains(const glm::vec3& point) const {
        return point.x >= Min.x && point.x <= Max.x &&
               point.y >= Min.y && point.y <= Max.y &&
               point.z >= Min.z && point.z <= Max.z;
    }

    bool Intersects(const AABB& other) const {
        return Min.x <= other.Max.x && Max.x >= other.Min.x &&
               Min.y <= other.Max.y && Max.y >= other.Min.y &&
               Min.z <= other.Max.z && Max.z >= other.Min.z;
    }

    /// 射线-AABB 交叉 (slab method)
    bool RayIntersect(const glm::vec3& origin, const glm::vec3& invDir,
                      float tMin = 0.0f, float tMax = 1e30f) const {
        for (int i = 0; i < 3; i++) {
            float t1 = (Min[i] - origin[i]) * invDir[i];
            float t2 = (Max[i] - origin[i]) * invDir[i];
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }
        return true;
    }
};

// ── 球体 ───────────────────────────────────────────────────

struct Sphere {
    glm::vec3 Center = {0, 0, 0};
    f32 Radius = 0.5f;

    /// 获取包围的 AABB
    AABB ToAABB() const {
        return { Center - glm::vec3(Radius), Center + glm::vec3(Radius) };
    }
};

// ── 胶囊体 ─────────────────────────────────────────────────

struct Capsule {
    glm::vec3 PointA = {0, -0.5f, 0};  // 底部球心
    glm::vec3 PointB = {0,  0.5f, 0};  // 顶部球心
    f32 Radius = 0.25f;

    /// 获取中心
    glm::vec3 Center() const { return (PointA + PointB) * 0.5f; }

    /// 获取高度（含两端球半径）
    f32 Height() const { return glm::length(PointB - PointA) + 2.0f * Radius; }

    /// 获取包围的 AABB
    AABB ToAABB() const {
        AABB aabb;
        aabb.Min = glm::min(PointA, PointB) - glm::vec3(Radius);
        aabb.Max = glm::max(PointA, PointB) + glm::vec3(Radius);
        return aabb;
    }

    /// 获取线段上离给定点最近的点
    static glm::vec3 ClosestPointOnSegment(const glm::vec3& p,
                                            const glm::vec3& a, const glm::vec3& b) {
        glm::vec3 ab = b - a;
        f32 t = glm::dot(p - a, ab) / glm::dot(ab, ab);
        t = glm::clamp(t, 0.0f, 1.0f);
        return a + ab * t;
    }
};

// ── 碰撞体形状枚举 ─────────────────────────────────────────

enum class ColliderShape : u8 {
    Box,       // AABB
    Sphere,    // 球
    Capsule,   // 胶囊
    OBB,       // 有向包围盒
};

// ── 射线 ────────────────────────────────────────────────────

struct Ray {
    glm::vec3 Origin    = {0, 0, 0};
    glm::vec3 Direction = {0, 0, -1};

    glm::vec3 At(f32 t) const { return Origin + Direction * t; }
};

// ── 碰撞结果 ────────────────────────────────────────────────

struct HitResult {
    bool Hit = false;
    f32  Distance = 0.0f;
    glm::vec3 Point = {0,0,0};
    glm::vec3 Normal = {0,1,0};
};

struct CollisionPair {
    u32 EntityA = 0;
    u32 EntityB = 0;
    glm::vec3 Normal = {0,0,0};
    f32 Penetration = 0.0f;
};

// ── 碰撞事件状态 ────────────────────────────────────────────

enum class CollisionState : u8 {
    Enter,   // 本帧刚开始碰撞
    Stay,    // 持续碰撞中
    Exit,    // 本帧刚结束碰撞
};

struct CollisionEventData {
    u32 EntityA = 0;
    u32 EntityB = 0;
    CollisionState State = CollisionState::Enter;
    glm::vec3 Normal = {0,0,0};
    f32 Penetration = 0.0f;
};

// ── 碰撞检测 ────────────────────────────────────────────────

class Collision {
public:
    /// AABB vs AABB
    static bool TestAABB(const AABB& a, const AABB& b);
    static bool TestAABB(const AABB& a, const AABB& b,
                         glm::vec3& outNormal, f32& outPenetration);

    /// 球 vs 球
    static bool TestSpheres(const Sphere& a, const Sphere& b,
                            glm::vec3& outNormal, f32& outPenetration);

    /// 球 vs AABB
    static bool TestSphereAABB(const Sphere& s, const AABB& b,
                               glm::vec3& outNormal, f32& outPenetration);

    /// 胶囊 vs 胶囊
    static bool TestCapsules(const Capsule& a, const Capsule& b,
                             glm::vec3& outNormal, f32& outPenetration);

    /// 胶囊 vs AABB
    static bool TestCapsuleAABB(const Capsule& cap, const AABB& aabb,
                                glm::vec3& outNormal, f32& outPenetration);

    /// 胶囊 vs 球
    static bool TestCapsuleSphere(const Capsule& cap, const Sphere& sph,
                                  glm::vec3& outNormal, f32& outPenetration);

    /// 点 vs 球
    static bool TestPointSphere(const glm::vec3& point,
                                const glm::vec3& center, f32 radius);

    /// 射线检测
    static HitResult RaycastAABB(const Ray& ray, const AABB& aabb);
    static HitResult RaycastSphere(const Ray& ray, const Sphere& sphere);
    static HitResult RaycastCapsule(const Ray& ray, const Capsule& capsule);
    static HitResult RaycastPlane(const Ray& ray, f32 height = 0.0f);

    /// 碰撞层检测：两个层是否可以碰撞
    static bool LayersCanCollide(u16 layerA, u16 maskA, u16 layerB, u16 maskB) {
        return (layerA & maskB) != 0 && (layerB & maskA) != 0;
    }
};

// ── 空间哈希网格 ────────────────────────────────────────────

class SpatialHash {
public:
    SpatialHash(f32 cellSize = 4.0f) : m_CellSize(cellSize) {}

    void Clear();
    void Insert(u32 entity, const AABB& worldAABB);
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
