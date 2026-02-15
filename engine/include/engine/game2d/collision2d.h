#pragma once

#include "engine/core/types.h"
#include "engine/game2d/tilemap.h"

#include <glm/glm.hpp>
#include <cmath>

namespace Engine {

// ── 2D AABB 碰撞盒 ────────────────────────────────────────
struct AABB2D {
    glm::vec2 Min;  // 左下角
    glm::vec2 Max;  // 右上角

    AABB2D() : Min(0), Max(0) {}
    AABB2D(const glm::vec2& center, const glm::vec2& halfSize)
        : Min(center - halfSize), Max(center + halfSize) {}

    bool Overlaps(const AABB2D& other) const {
        return Min.x < other.Max.x && Max.x > other.Min.x &&
               Min.y < other.Max.y && Max.y > other.Min.y;
    }

    glm::vec2 GetCenter() const { return (Min + Max) * 0.5f; }
    glm::vec2 GetSize()   const { return Max - Min; }
};

// ── 2D 碰撞工具 ───────────────────────────────────────────

namespace Collision2D {

/// 分轴 Tilemap 碰撞移动 — 返回修正后的新位置
/// @param tilemap  瓦片地图引用
/// @param oldPos   当前位置 (中心)
/// @param newPos   目标位置 (中心)
/// @param halfSize 碰撞体半尺寸
inline glm::vec2 MoveAndSlide(const Tilemap& tilemap,
                               const glm::vec2& oldPos,
                               const glm::vec2& newPos,
                               const glm::vec2& halfSize)
{
    glm::vec2 result = oldPos;
    u32 w = tilemap.GetWidth();
    u32 h = tilemap.GetHeight();

    // 辅助 lambda: 检查 AABB 四角是否碰到 Solid
    auto isSolidAt = [&](f32 cx, f32 cy) -> bool {
        // 检测 AABB 四角覆盖的 tile
        i32 x0 = (i32)std::floor(cx - halfSize.x);
        i32 y0 = (i32)std::floor(cy - halfSize.y);
        i32 x1 = (i32)std::floor(cx + halfSize.x - 0.001f);
        i32 y1 = (i32)std::floor(cy + halfSize.y - 0.001f);

        for (i32 ty = y0; ty <= y1; ty++) {
            for (i32 tx = x0; tx <= x1; tx++) {
                if (tx < 0 || ty < 0 || tx >= (i32)w || ty >= (i32)h)
                    return true;  // 越界视为 Solid
                if (tilemap.IsSolid((u32)tx, (u32)ty))
                    return true;
            }
        }
        return false;
    };

    // 分轴 X
    if (!isSolidAt(newPos.x, oldPos.y))
        result.x = newPos.x;

    // 分轴 Y
    if (!isSolidAt(result.x, newPos.y))
        result.y = newPos.y;

    return result;
}

/// 圆形推挤 — 两个圆形碰撞体之间的推开方向和距离
/// 返回从 A 指向 B 的推开向量 (应用到 B 上使其远离 A)
/// 如果没有碰撞返回 {0, 0}
inline glm::vec2 CirclePush(const glm::vec2& posA, f32 radiusA,
                             const glm::vec2& posB, f32 radiusB)
{
    glm::vec2 diff = posB - posA;
    f32 dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    f32 minDist = radiusA + radiusB;

    if (dist >= minDist || dist < 0.0001f)
        return {0, 0};

    glm::vec2 dir = diff / dist;
    f32 overlap = minDist - dist;
    return dir * overlap;
}

} // namespace Collision2D
} // namespace Engine
