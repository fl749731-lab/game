#include "engine/renderer/fps_camera_controller.h"
#include "engine/core/log.h"

#include <glm/glm.hpp>

namespace Engine {

FPSCameraController::FPSCameraController(PerspectiveCamera* camera, const FPSCameraConfig& config)
    : m_Camera(camera), m_Config(config)
{
}

void FPSCameraController::SetCaptured(bool captured) {
    m_Captured = captured;
    Input::SetCursorMode(captured ? CursorMode::Captured : CursorMode::Normal);
}

void FPSCameraController::Update(f32 dt) {
    if (!m_Camera) return;

    // F2 切换捕获
    if (Input::IsKeyJustPressed(Key::F2)) {
        SetCaptured(!m_Captured);
    }

    // Esc 释放鼠标
    if (Input::IsKeyJustPressed(Key::Escape) && m_Captured) {
        SetCaptured(false);
    }

    // ── 移动 ────────────────────────────────────────────────
    f32 speed = m_Config.MoveSpeed;
    if (Input::IsKeyDown(Key::LeftShift)) speed *= m_Config.SprintMulti;

    glm::vec3 pos = m_Camera->GetPosition();
    if (Input::IsKeyDown(Key::W)) pos += m_Camera->GetForward() * speed * dt;
    if (Input::IsKeyDown(Key::S)) pos -= m_Camera->GetForward() * speed * dt;
    if (Input::IsKeyDown(Key::A)) pos -= m_Camera->GetRight() * speed * dt;
    if (Input::IsKeyDown(Key::D)) pos += m_Camera->GetRight() * speed * dt;
    if (Input::IsKeyDown(Key::Space))     pos.y += speed * dt;
    if (Input::IsKeyDown(Key::LeftCtrl))  pos.y -= speed * dt;
    m_Camera->SetPosition(pos);

    // ── 旋转 ────────────────────────────────────────────────
    f32 yaw = m_Camera->GetYaw();
    f32 pitch = m_Camera->GetPitch();

    // 键盘旋转
    if (Input::IsKeyDown(Key::Left))  yaw   -= m_Config.LookSpeed * dt;
    if (Input::IsKeyDown(Key::Right)) yaw   += m_Config.LookSpeed * dt;
    if (Input::IsKeyDown(Key::Up))    pitch += m_Config.LookSpeed * dt;
    if (Input::IsKeyDown(Key::Down))  pitch -= m_Config.LookSpeed * dt;

    // 鼠标旋转（仅捕获模式）
    if (m_Captured) {
        yaw   += Input::GetMouseDeltaX() * m_Config.MouseSens;
        pitch += Input::GetMouseDeltaY() * m_Config.MouseSens;
    }

    pitch = glm::clamp(pitch, m_Config.MinPitch, m_Config.MaxPitch);
    m_Camera->SetRotation(yaw, pitch);

    // ── FOV 缩放 ────────────────────────────────────────────
    if (Input::IsKeyDown(Key::Z)) m_Camera->Zoom(dt * m_Config.ZoomSpeed);
    if (Input::IsKeyDown(Key::X)) m_Camera->Zoom(-dt * m_Config.ZoomSpeed);

    f32 scroll = Input::GetScrollOffset();
    if (scroll != 0.0f) m_Camera->Zoom(scroll * m_Config.ScrollZoomStep);
}

} // namespace Engine
