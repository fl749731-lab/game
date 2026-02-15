#pragma once

#include "engine/core/types.h"
#include "engine/renderer/camera.h"

#include <glm/glm.hpp>

namespace Engine {

// ── 2D 相机控制器 ──────────────────────────────────────────
// 平滑跟随目标，支持死区 + 地图边界限制

class Camera2DController {
public:
    Camera2DController() = default;
    Camera2DController(f32 viewWidth, f32 viewHeight);

    /// 每帧更新 (传入目标世界坐标)
    void Update(f32 dt, const glm::vec2& targetPos);

    /// 获取当前相机位置 (左下角)
    const glm::vec2& GetPosition() const { return m_Position; }

    /// 应用到正交相机
    void ApplyTo(OrthographicCamera& camera) const;

    // ── 配置 ───────────────────────────────────────────────

    /// 视口尺寸 (世界单位，非像素)
    void SetViewSize(f32 w, f32 h) { m_ViewW = w; m_ViewH = h; }

    /// 跟随平滑度 (越大越快, 1=立即到达)
    void SetSmoothness(f32 s) { m_Smoothness = s; }

    /// 死区 (目标在此范围内不移动相机)
    void SetDeadZone(const glm::vec2& dz) { m_DeadZone = dz; }

    /// 世界边界 (相机不超出范围)
    void SetWorldBounds(const glm::vec2& min, const glm::vec2& max) {
        m_BoundsMin = min;
        m_BoundsMax = max;
        m_UseBounds = true;
    }

    void ClearWorldBounds() { m_UseBounds = false; }

    /// 缩放
    void SetZoom(f32 z) { m_Zoom = z; }
    f32  GetZoom() const { return m_Zoom; }

    /// 屏幕震动
    void Shake(f32 intensity, f32 duration);

private:
    glm::vec2 m_Position   = {0, 0};
    f32       m_ViewW      = 20.0f;   // 世界单位
    f32       m_ViewH      = 15.0f;
    f32       m_Smoothness = 5.0f;
    f32       m_Zoom       = 1.0f;
    glm::vec2 m_DeadZone   = {0.5f, 0.5f};

    bool      m_UseBounds  = false;
    glm::vec2 m_BoundsMin  = {0, 0};
    glm::vec2 m_BoundsMax  = {100, 100};

    // 屏幕震动
    f32       m_ShakeIntensity = 0.0f;
    f32       m_ShakeTimer     = 0.0f;
    glm::vec2 m_ShakeOffset    = {0, 0};
};

} // namespace Engine
