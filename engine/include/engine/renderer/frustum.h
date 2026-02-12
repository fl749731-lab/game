#pragma once

#include "engine/core/types.h"
#include "engine/physics/collision.h"

#include <glm/glm.hpp>

namespace Engine {

// ── 视锥体 ──────────────────────────────────────────────────
// 从 View-Projection 矩阵提取 6 个平面，用于 AABB 剔除

struct Plane {
    glm::vec3 Normal = {0, 1, 0};
    f32 Distance = 0.0f;

    f32 DistanceToPoint(const glm::vec3& p) const {
        return glm::dot(Normal, p) + Distance;
    }
};

class Frustum {
public:
    /// 从 VP 矩阵提取 6 个平面
    void ExtractFromVP(const glm::mat4& vp);

    /// AABB 是否在视锥体内（或相交）
    bool IsAABBVisible(const AABB& aabb) const;

    /// 球体是否在视锥体内
    bool IsSphereVisible(const glm::vec3& center, f32 radius) const;

    /// 点是否在视锥体内
    bool IsPointVisible(const glm::vec3& point) const;

private:
    enum Side { LEFT = 0, RIGHT, BOTTOM, TOP, NEAR_P, FAR_P, COUNT };
    Plane m_Planes[COUNT];
};

} // namespace Engine
