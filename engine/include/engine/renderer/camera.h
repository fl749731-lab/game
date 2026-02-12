#pragma once

#include "engine/core/types.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Engine {

// ── 正交摄像机 (2D) ─────────────────────────────────────────

class OrthographicCamera {
public:
    OrthographicCamera(f32 left, f32 right, f32 bottom, f32 top);

    void SetProjection(f32 left, f32 right, f32 bottom, f32 top);

    const glm::vec3& GetPosition() const { return m_Position; }
    void SetPosition(const glm::vec3& pos) { m_Position = pos; RecalculateView(); }

    f32 GetRotation() const { return m_Rotation; }
    void SetRotation(f32 rotation) { m_Rotation = rotation; RecalculateView(); }

    const glm::mat4& GetProjectionMatrix() const { return m_Projection; }
    const glm::mat4& GetViewMatrix() const { return m_View; }
    const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjection; }

private:
    void RecalculateView();

    glm::mat4 m_Projection;
    glm::mat4 m_View;
    glm::mat4 m_ViewProjection;

    glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
    f32 m_Rotation = 0.0f;
};

// ── 透视摄像机 (3D) ─────────────────────────────────────────

class PerspectiveCamera {
public:
    PerspectiveCamera(f32 fov, f32 aspect, f32 nearClip, f32 farClip);

    void SetProjection(f32 fov, f32 aspect, f32 nearClip, f32 farClip);

    const glm::vec3& GetPosition() const { return m_Position; }
    void SetPosition(const glm::vec3& pos) { m_Position = pos; RecalculateView(); }

    f32 GetYaw() const { return m_Yaw; }
    f32 GetPitch() const { return m_Pitch; }
    void SetRotation(f32 yaw, f32 pitch) { m_Yaw = yaw; m_Pitch = pitch; RecalculateView(); }

    glm::vec3 GetForward() const;
    glm::vec3 GetRight() const;
    glm::vec3 GetUp() const;

    f32 GetFOV() const { return m_Fov; }
    f32 GetNearClip() const { return m_NearClip; }
    f32 GetFarClip() const { return m_FarClip; }
    f32 GetAspect() const { return m_Aspect; }
    void SetFOV(f32 fov);
    void Zoom(f32 delta);

    const glm::mat4& GetProjectionMatrix() const { return m_Projection; }
    const glm::mat4& GetViewMatrix() const { return m_View; }
    const glm::mat4& GetViewProjectionMatrix() const { return m_ViewProjection; }

private:
    void RecalculateView();

    glm::mat4 m_Projection;
    glm::mat4 m_View;
    glm::mat4 m_ViewProjection;

    glm::vec3 m_Position = { 0.0f, 0.0f, 3.0f };
    f32 m_Yaw = -90.0f;   // 面向 -Z
    f32 m_Pitch = 0.0f;
    f32 m_Fov = 45.0f;
    f32 m_Aspect = 16.0f / 9.0f;
    f32 m_NearClip = 0.1f;
    f32 m_FarClip = 100.0f;
};

} // namespace Engine
