#include "engine/physics/obb.h"
#include "engine/core/log.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <limits>
#include <cmath>

namespace Engine {

// ═══════════════════════════════════════════════════════════
//  OBB 实现
// ═══════════════════════════════════════════════════════════

OBB OBB::FromTransform(const glm::vec3& pos,
                       const glm::vec3& scale,
                       const glm::quat& rotation) {
    OBB obb;
    obb.Center   = pos;
    obb.HalfSize = scale * 0.5f;
    obb.Axes     = glm::mat3_cast(rotation);
    return obb;
}

void OBB::GetCorners(glm::vec3 corners[8]) const {
    int idx = 0;
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            for (int sz = -1; sz <= 1; sz += 2) {
                corners[idx++] = Center
                    + Axes[0] * HalfSize.x * (f32)sx
                    + Axes[1] * HalfSize.y * (f32)sy
                    + Axes[2] * HalfSize.z * (f32)sz;
            }
        }
    }
}

AABB OBB::ToAABB() const {
    AABB result;
    glm::vec3 corners[8];
    GetCorners(corners);
    result.Min = corners[0];
    result.Max = corners[0];
    for (int i = 1; i < 8; i++) {
        result.Min = glm::min(result.Min, corners[i]);
        result.Max = glm::max(result.Max, corners[i]);
    }
    return result;
}

glm::vec3 OBB::ClosestPoint(const glm::vec3& point) const {
    glm::vec3 d = point - Center;
    glm::vec3 result = Center;
    for (int i = 0; i < 3; i++) {
        f32 dist = glm::dot(d, Axes[i]);
        dist = glm::clamp(dist, -HalfSize[i], HalfSize[i]);
        result += Axes[i] * dist;
    }
    return result;
}

void OBB::ProjectOntoAxis(const glm::vec3& axis, f32& outMin, f32& outMax) const {
    f32 centerProj = glm::dot(Center, axis);
    f32 extent = std::abs(glm::dot(Axes[0] * HalfSize.x, axis))
               + std::abs(glm::dot(Axes[1] * HalfSize.y, axis))
               + std::abs(glm::dot(Axes[2] * HalfSize.z, axis));
    outMin = centerProj - extent;
    outMax = centerProj + extent;
}

// ═══════════════════════════════════════════════════════════
//  GJK 支撑函数
// ═══════════════════════════════════════════════════════════

glm::vec3 GJK::SupportOBB(const OBB& obb, const glm::vec3& dir) {
    // 找到 OBB 上在给定方向上最远的点
    glm::vec3 result = obb.Center;
    for (int i = 0; i < 3; i++) {
        f32 projection = glm::dot(dir, obb.Axes[i]);
        result += obb.Axes[i] * obb.HalfSize[i] * (projection >= 0 ? 1.0f : -1.0f);
    }
    return result;
}

glm::vec3 GJK::SupportSphere(const Sphere& s, const glm::vec3& dir) {
    return s.Center + glm::normalize(dir) * s.Radius;
}

// ── GJK Minkowski 差支撑点 ──────────────────────────────────

static SupportPoint MinkowskiSupport(const OBB& a, const OBB& b, const glm::vec3& dir) {
    SupportPoint sp;
    sp.A = GJK::SupportOBB(a, dir);
    sp.B = GJK::SupportOBB(b, -dir);
    sp.Point = sp.A - sp.B;
    return sp;
}

// ═══════════════════════════════════════════════════════════
//  GJK Simplex 处理
// ═══════════════════════════════════════════════════════════

bool GJK::Line(std::vector<SupportPoint>& simplex, glm::vec3& dir) {
    glm::vec3 a = simplex[1].Point;
    glm::vec3 b = simplex[0].Point;
    glm::vec3 ab = b - a;
    glm::vec3 ao = -a;
    if (glm::dot(ab, ao) > 0) {
        dir = glm::cross(glm::cross(ab, ao), ab);
    } else {
        simplex = {simplex[1]};
        dir = ao;
    }
    return false;
}

bool GJK::Triangle(std::vector<SupportPoint>& simplex, glm::vec3& dir) {
    glm::vec3 a = simplex[2].Point;
    glm::vec3 b = simplex[1].Point;
    glm::vec3 c = simplex[0].Point;
    glm::vec3 ab = b - a, ac = c - a, ao = -a;
    glm::vec3 abc = glm::cross(ab, ac);

    if (glm::dot(glm::cross(abc, ac), ao) > 0) {
        if (glm::dot(ac, ao) > 0) {
            simplex = {simplex[0], simplex[2]};
            dir = glm::cross(glm::cross(ac, ao), ac);
        } else {
            simplex = {simplex[1], simplex[2]};
            return Line(simplex, dir);
        }
    } else {
        if (glm::dot(glm::cross(ab, abc), ao) > 0) {
            simplex = {simplex[1], simplex[2]};
            return Line(simplex, dir);
        } else {
            if (glm::dot(abc, ao) > 0) {
                dir = abc;
            } else {
                simplex = {simplex[1], simplex[0], simplex[2]};
                dir = -abc;
            }
        }
    }
    return false;
}

bool GJK::Tetrahedron(std::vector<SupportPoint>& simplex, glm::vec3& dir) {
    glm::vec3 a = simplex[3].Point;
    glm::vec3 b = simplex[2].Point;
    glm::vec3 c = simplex[1].Point;
    glm::vec3 d = simplex[0].Point;
    glm::vec3 ab = b - a, ac = c - a, ad = d - a, ao = -a;

    glm::vec3 abc = glm::cross(ab, ac);
    glm::vec3 acd = glm::cross(ac, ad);
    glm::vec3 adb = glm::cross(ad, ab);

    if (glm::dot(abc, ao) > 0) {
        simplex = {simplex[1], simplex[2], simplex[3]};
        return Triangle(simplex, dir);
    }
    if (glm::dot(acd, ao) > 0) {
        simplex = {simplex[0], simplex[1], simplex[3]};
        return Triangle(simplex, dir);
    }
    if (glm::dot(adb, ao) > 0) {
        simplex = {simplex[2], simplex[0], simplex[3]};
        return Triangle(simplex, dir);
    }
    return true; // 原点在四面体内部
}

bool GJK::DoSimplex(std::vector<SupportPoint>& simplex, glm::vec3& direction) {
    switch (simplex.size()) {
        case 2: return Line(simplex, direction);
        case 3: return Triangle(simplex, direction);
        case 4: return Tetrahedron(simplex, direction);
    }
    return false;
}

// ═══════════════════════════════════════════════════════════
//  GJK 主循环
// ═══════════════════════════════════════════════════════════

bool GJK::GJKIntersect(const OBB& a, const OBB& b,
                        std::vector<SupportPoint>& simplex) {
    glm::vec3 direction = glm::normalize(b.Center - a.Center);
    if (glm::length(direction) < 0.0001f) direction = glm::vec3(1, 0, 0);

    SupportPoint support = MinkowskiSupport(a, b, direction);
    simplex.push_back(support);
    direction = -support.Point;

    const int MAX_ITER = 64;
    for (int i = 0; i < MAX_ITER; i++) {
        support = MinkowskiSupport(a, b, direction);
        if (glm::dot(support.Point, direction) < 0) return false;
        simplex.push_back(support);
        if (DoSimplex(simplex, direction)) return true;
    }
    return false;
}

// ═══════════════════════════════════════════════════════════
//  EPA 穿透计算
// ═══════════════════════════════════════════════════════════

struct EPATriangle {
    int a, b, c;
    glm::vec3 normal;
    f32 distance;
};

static void ComputeEPATriangle(EPATriangle& tri,
                                const std::vector<SupportPoint>& polytope) {
    glm::vec3 ab = polytope[tri.b].Point - polytope[tri.a].Point;
    glm::vec3 ac = polytope[tri.c].Point - polytope[tri.a].Point;
    tri.normal = glm::normalize(glm::cross(ab, ac));
    tri.distance = glm::dot(tri.normal, polytope[tri.a].Point);
    if (tri.distance < 0) {
        tri.normal = -tri.normal;
        tri.distance = -tri.distance;
        std::swap(tri.b, tri.c);
    }
}

GJKResult GJK::EPA(const OBB& a, const OBB& b,
                    std::vector<SupportPoint>& simplex) {
    GJKResult result;
    result.Colliding = true;

    std::vector<SupportPoint> polytope = simplex;
    std::vector<EPATriangle> faces;

    // 初始 4 面体 → 4 面
    int triIndices[4][3] = {{0,1,2}, {0,2,3}, {0,3,1}, {1,3,2}};
    for (auto& tri : triIndices) {
        EPATriangle t{tri[0], tri[1], tri[2], {}, 0};
        ComputeEPATriangle(t, polytope);
        faces.push_back(t);
    }

    const int MAX_ITER = 64;
    const f32 EPA_EPSILON = 0.0001f;

    for (int iteration = 0; iteration < MAX_ITER; iteration++) {
        // 找距原点最近的面
        int minFace = 0;
        f32 minDist = faces[0].distance;
        for (int i = 1; i < (int)faces.size(); i++) {
            if (faces[i].distance < minDist) {
                minDist = faces[i].distance;
                minFace = i;
            }
        }

        glm::vec3 searchDir = faces[minFace].normal;
        SupportPoint newPoint = MinkowskiSupport(a, b, searchDir);
        f32 newDist = glm::dot(newPoint.Point, searchDir);

        if (newDist - minDist < EPA_EPSILON) {
            result.Normal = faces[minFace].normal;
            result.Penetration = minDist;
            // 近似接触点
            result.ContactPoint = (newPoint.A + newPoint.B) * 0.5f;
            return result;
        }

        // 展开多面体：移除可见面，添加新面
        int newIdx = (int)polytope.size();
        polytope.push_back(newPoint);

        std::vector<std::pair<int,int>> edges;
        for (int i = (int)faces.size() - 1; i >= 0; i--) {
            if (glm::dot(faces[i].normal, newPoint.Point - polytope[faces[i].a].Point) > 0) {
                // 这个面对新点可见，收集边
                auto addEdge = [&](int e1, int e2) {
                    for (auto it = edges.begin(); it != edges.end(); it++) {
                        if (it->first == e2 && it->second == e1) {
                            edges.erase(it);
                            return;
                        }
                    }
                    edges.push_back({e1, e2});
                };
                addEdge(faces[i].a, faces[i].b);
                addEdge(faces[i].b, faces[i].c);
                addEdge(faces[i].c, faces[i].a);
                faces.erase(faces.begin() + i);
            }
        }

        for (auto& [e1, e2] : edges) {
            EPATriangle t{e1, e2, newIdx, {}, 0};
            ComputeEPATriangle(t, polytope);
            faces.push_back(t);
        }
    }

    // 超出迭代次数，返回最佳估计
    int minFace = 0;
    f32 minDist = faces[0].distance;
    for (int i = 1; i < (int)faces.size(); i++) {
        if (faces[i].distance < minDist) {
            minDist = faces[i].distance;
            minFace = i;
        }
    }
    result.Normal = faces[minFace].normal;
    result.Penetration = minDist;
    return result;
}

// ═══════════════════════════════════════════════════════════
//  GJK 公开接口
// ═══════════════════════════════════════════════════════════

GJKResult GJK::TestOBB(const OBB& a, const OBB& b) {
    std::vector<SupportPoint> simplex;
    if (!GJKIntersect(a, b, simplex)) {
        return {false, {0,1,0}, 0, {0,0,0}};
    }
    return EPA(a, b, simplex);
}

GJKResult GJK::TestOBBSphere(const OBB& obb, const Sphere& sphere) {
    // OBB 上最近点到球心的碰撞检测
    glm::vec3 closest = obb.ClosestPoint(sphere.Center);
    glm::vec3 diff = sphere.Center - closest;
    f32 dist2 = glm::dot(diff, diff);
    f32 r2 = sphere.Radius * sphere.Radius;

    GJKResult result;
    if (dist2 > r2) return result;

    result.Colliding = true;
    f32 dist = std::sqrt(dist2);
    if (dist > 0.0001f) {
        result.Normal = diff / dist;
    } else {
        result.Normal = glm::vec3(0, 1, 0);
    }
    result.Penetration = sphere.Radius - dist;
    result.ContactPoint = closest;
    return result;
}

// ═══════════════════════════════════════════════════════════
//  SAT 实现 (15 轴)
// ═══════════════════════════════════════════════════════════

bool SAT::TestOBB(const OBB& a, const OBB& b) {
    glm::vec3 n; f32 p;
    return TestOBB(a, b, n, p);
}

bool SAT::TestOBB(const OBB& a, const OBB& b,
                  glm::vec3& outNormal, f32& outPenetration) {
    f32 minOverlap = std::numeric_limits<f32>::max();
    glm::vec3 minAxis(0, 1, 0);

    auto testAxis = [&](const glm::vec3& axis) -> bool {
        if (glm::dot(axis, axis) < 0.0001f) return true;
        glm::vec3 normAxis = glm::normalize(axis);

        f32 aMin, aMax, bMin, bMax;
        a.ProjectOntoAxis(normAxis, aMin, aMax);
        b.ProjectOntoAxis(normAxis, bMin, bMax);

        f32 overlap = std::min(aMax, bMax) - std::max(aMin, bMin);
        if (overlap <= 0) return false;

        if (overlap < minOverlap) {
            minOverlap = overlap;
            minAxis = normAxis;
        }
        return true;
    };

    // 6 面法线 (3 + 3)
    for (int i = 0; i < 3; i++) {
        if (!testAxis(a.Axes[i])) return false;
        if (!testAxis(b.Axes[i])) return false;
    }

    // 9 叉积轴
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (!testAxis(glm::cross(a.Axes[i], b.Axes[j]))) return false;
        }
    }

    // 确保法线方向从 A → B
    glm::vec3 ab = b.Center - a.Center;
    if (glm::dot(minAxis, ab) < 0) minAxis = -minAxis;

    outNormal = minAxis;
    outPenetration = minOverlap;
    return true;
}

} // namespace Engine
