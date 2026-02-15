#include "engine/game2d/camera2d_controller.h"

#include <cmath>
#include <cstdlib>

namespace Engine {

Camera2DController::Camera2DController(f32 viewWidth, f32 viewHeight)
    : m_ViewW(viewWidth), m_ViewH(viewHeight) {}

void Camera2DController::Update(f32 dt, const glm::vec2& targetPos) {
    // 屏幕震动衰减
    if (m_ShakeTimer > 0.0f) {
        m_ShakeTimer -= dt;
        f32 t = m_ShakeTimer > 0.0f ? m_ShakeIntensity : 0.0f;
        m_ShakeOffset.x = ((f32)(rand() % 200 - 100) / 100.0f) * t;
        m_ShakeOffset.y = ((f32)(rand() % 200 - 100) / 100.0f) * t;
    } else {
        m_ShakeOffset = {0, 0};
    }

    // 死区计算
    glm::vec2 diff = targetPos - m_Position;
    glm::vec2 move = {0, 0};

    if (std::abs(diff.x) > m_DeadZone.x)
        move.x = diff.x - (diff.x > 0 ? m_DeadZone.x : -m_DeadZone.x);
    if (std::abs(diff.y) > m_DeadZone.y)
        move.y = diff.y - (diff.y > 0 ? m_DeadZone.y : -m_DeadZone.y);

    // 平滑插值
    f32 t = 1.0f - std::exp(-m_Smoothness * dt);
    m_Position += move * t;

    // 应用世界边界
    if (m_UseBounds) {
        f32 hw = m_ViewW * 0.5f / m_Zoom;
        f32 hh = m_ViewH * 0.5f / m_Zoom;

        m_Position.x = std::max(m_BoundsMin.x + hw, std::min(m_BoundsMax.x - hw, m_Position.x));
        m_Position.y = std::max(m_BoundsMin.y + hh, std::min(m_BoundsMax.y - hh, m_Position.y));
    }
}

void Camera2DController::ApplyTo(OrthographicCamera& camera) const {
    f32 hw = m_ViewW * 0.5f / m_Zoom;
    f32 hh = m_ViewH * 0.5f / m_Zoom;

    glm::vec2 pos = m_Position + m_ShakeOffset;

    camera.SetProjection(pos.x - hw, pos.x + hw,
                         pos.y - hh, pos.y + hh);
    camera.SetPosition({pos.x, pos.y, 0.0f});
}

void Camera2DController::Shake(f32 intensity, f32 duration) {
    m_ShakeIntensity = intensity;
    m_ShakeTimer = duration;
}

} // namespace Engine
