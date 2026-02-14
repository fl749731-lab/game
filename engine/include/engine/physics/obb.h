#pragma once

#include "engine/core/types.h"
#include "engine/physics/collision.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Engine {

// ── 有向包围盒 (OBB) ────────────────────────────────────────
//
// 相比 AABB 支持任意旋转，用于更精确的碰撞检测
// 与 GJK 算法配合使用

struct OBB {
    glm::vec3 Center   = {0, 0, 0};
    glm::vec3 HalfSize = {0.5f, 0.5f, 0.5f};
    glm::mat3 Axes     = glm::mat3(1.0f);  // 3 个正交轴（列向量）

    /// 从 Transform 构建 OBB
    static OBB FromTransform(const glm::vec3& pos,
                             const glm::vec3& scale,
                             const glm::quat& rotation);

    /// 获取 8 个顶点（世界空间）
    void GetCorners(glm::vec3 corners[8]) const;

    /// 获取包围 AABB
    AABB ToAABB() const;

    /// 获取离给定点最近的 OBB 表面点
    glm::vec3 ClosestPoint(const glm::vec3& point) const;

    /// 用向量投影到指定轴上的区间
    void ProjectOntoAxis(const glm::vec3& axis, f32& outMin, f32& outMax) const;
};

// ── Minkowski 支撑点 ────────────────────────────────────────

struct SupportPoint {
    glm::vec3 Point;    // Minkowski 差上的点
    glm::vec3 A;        // 形状 A 上的支撑点
    glm::vec3 B;        // 形状 B 上的支撑点
};

// ── GJK 碰撞检测结果 ────────────────────────────────────────

struct GJKResult {
    bool Colliding = false;
    glm::vec3 Normal = {0, 1, 0};
    f32 Penetration = 0.0f;
    glm::vec3 ContactPoint = {0, 0, 0};
};

// ── GJK/EPA 碰撞检测算法 ────────────────────────────────────
//
// GJK: Gilbert-Johnson-Keerthi 凸体碰撞检测
// EPA: Expanding Polytope Algorithm 穿透深度/法线计算

class GJK {
public:
    /// OBB vs OBB 碰撞检测 (GJK + EPA)
    static GJKResult TestOBB(const OBB& a, const OBB& b);

    /// OBB vs Sphere
    static GJKResult TestOBBSphere(const OBB& obb, const Sphere& sphere);

    /// 通用凸体支撑函数
    static glm::vec3 SupportOBB(const OBB& obb, const glm::vec3& dir);
    static glm::vec3 SupportSphere(const Sphere& s, const glm::vec3& dir);

private:
    /// GJK 主循环
    static bool GJKIntersect(const OBB& a, const OBB& b,
                              std::vector<SupportPoint>& simplex);

    /// EPA 穿透计算
    static GJKResult EPA(const OBB& a, const OBB& b,
                          std::vector<SupportPoint>& simplex);

    /// Simplex 操作
    static bool DoSimplex(std::vector<SupportPoint>& simplex, glm::vec3& direction);
    static bool Line(std::vector<SupportPoint>& simplex, glm::vec3& dir);
    static bool Triangle(std::vector<SupportPoint>& simplex, glm::vec3& dir);
    static bool Tetrahedron(std::vector<SupportPoint>& simplex, glm::vec3& dir);
};

// ── SAT (分离轴定理) 快速 OBB 碰撞检测 ─────────────────────
// 用于粗筛，比 GJK 快但无法提供接触信息

class SAT {
public:
    /// OBB vs OBB 快速碰撞检测 (15 轴测试)
    static bool TestOBB(const OBB& a, const OBB& b);

    /// OBB vs OBB 带穿透信息
    static bool TestOBB(const OBB& a, const OBB& b,
                        glm::vec3& outNormal, f32& outPenetration);
};

} // namespace Engine
