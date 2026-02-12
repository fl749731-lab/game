#include "engine/renderer/frustum.h"

#include <cmath>

namespace Engine {

// ── 从 VP 矩阵提取平面 (Gribb-Hartmann 方法) ──────────────

void Frustum::ExtractFromVP(const glm::mat4& vp) {
    // Left
    m_Planes[LEFT].Normal.x = vp[0][3] + vp[0][0];
    m_Planes[LEFT].Normal.y = vp[1][3] + vp[1][0];
    m_Planes[LEFT].Normal.z = vp[2][3] + vp[2][0];
    m_Planes[LEFT].Distance = vp[3][3] + vp[3][0];

    // Right
    m_Planes[RIGHT].Normal.x = vp[0][3] - vp[0][0];
    m_Planes[RIGHT].Normal.y = vp[1][3] - vp[1][0];
    m_Planes[RIGHT].Normal.z = vp[2][3] - vp[2][0];
    m_Planes[RIGHT].Distance = vp[3][3] - vp[3][0];

    // Bottom
    m_Planes[BOTTOM].Normal.x = vp[0][3] + vp[0][1];
    m_Planes[BOTTOM].Normal.y = vp[1][3] + vp[1][1];
    m_Planes[BOTTOM].Normal.z = vp[2][3] + vp[2][1];
    m_Planes[BOTTOM].Distance = vp[3][3] + vp[3][1];

    // Top
    m_Planes[TOP].Normal.x = vp[0][3] - vp[0][1];
    m_Planes[TOP].Normal.y = vp[1][3] - vp[1][1];
    m_Planes[TOP].Normal.z = vp[2][3] - vp[2][1];
    m_Planes[TOP].Distance = vp[3][3] - vp[3][1];

    // Near
    m_Planes[NEAR_P].Normal.x = vp[0][3] + vp[0][2];
    m_Planes[NEAR_P].Normal.y = vp[1][3] + vp[1][2];
    m_Planes[NEAR_P].Normal.z = vp[2][3] + vp[2][2];
    m_Planes[NEAR_P].Distance = vp[3][3] + vp[3][2];

    // Far
    m_Planes[FAR_P].Normal.x = vp[0][3] - vp[0][2];
    m_Planes[FAR_P].Normal.y = vp[1][3] - vp[1][2];
    m_Planes[FAR_P].Normal.z = vp[2][3] - vp[2][2];
    m_Planes[FAR_P].Distance = vp[3][3] - vp[3][2];

    // 归一化所有平面
    for (int i = 0; i < COUNT; i++) {
        f32 len = glm::length(m_Planes[i].Normal);
        if (len > 0.0001f) {
            m_Planes[i].Normal /= len;
            m_Planes[i].Distance /= len;
        }
    }
}

// ── AABB 可见性测试 ─────────────────────────────────────────

bool Frustum::IsAABBVisible(const AABB& aabb) const {
    for (int i = 0; i < COUNT; i++) {
        // 取 AABB 在平面法线方向上最远的正顶点 (p-vertex)
        glm::vec3 pVertex;
        pVertex.x = (m_Planes[i].Normal.x >= 0) ? aabb.Max.x : aabb.Min.x;
        pVertex.y = (m_Planes[i].Normal.y >= 0) ? aabb.Max.y : aabb.Min.y;
        pVertex.z = (m_Planes[i].Normal.z >= 0) ? aabb.Max.z : aabb.Min.z;

        if (m_Planes[i].DistanceToPoint(pVertex) < 0.0f) {
            return false; // AABB 完全在这个平面外侧
        }
    }
    return true;
}

// ── 球体可见性测试 ──────────────────────────────────────────

bool Frustum::IsSphereVisible(const glm::vec3& center, f32 radius) const {
    for (int i = 0; i < COUNT; i++) {
        if (m_Planes[i].DistanceToPoint(center) < -radius) {
            return false;
        }
    }
    return true;
}

// ── 点可见性测试 ────────────────────────────────────────────

bool Frustum::IsPointVisible(const glm::vec3& point) const {
    for (int i = 0; i < COUNT; i++) {
        if (m_Planes[i].DistanceToPoint(point) < 0.0f) {
            return false;
        }
    }
    return true;
}

} // namespace Engine
