#include "engine/renderer/camera.h"
#include <cmath>

namespace Engine {

// ── OrthographicCamera ──────────────────────────────────────

OrthographicCamera::OrthographicCamera(f32 left, f32 right, f32 bottom, f32 top) 
    : m_Projection(glm::ortho(left, right, bottom, top, -1.0f, 1.0f))
    , m_View(1.0f)
{
    m_ViewProjection = m_Projection * m_View;
}

void OrthographicCamera::SetProjection(f32 left, f32 right, f32 bottom, f32 top) {
    m_Projection = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
    m_ViewProjection = m_Projection * m_View;
}

void OrthographicCamera::RecalculateView() {
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), m_Position)
        * glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation), glm::vec3(0, 0, 1));

    m_View = glm::inverse(transform);
    m_ViewProjection = m_Projection * m_View;
}

// ── PerspectiveCamera ───────────────────────────────────────

PerspectiveCamera::PerspectiveCamera(f32 fov, f32 aspect, f32 nearClip, f32 farClip)
    : m_Fov(fov)
    , m_Projection(glm::perspective(glm::radians(fov), aspect, nearClip, farClip))
    , m_View(1.0f)
{
    RecalculateView();
}

void PerspectiveCamera::SetProjection(f32 fov, f32 aspect, f32 nearClip, f32 farClip) {
    m_Fov = fov;
    m_Projection = glm::perspective(glm::radians(fov), aspect, nearClip, farClip);
    m_ViewProjection = m_Projection * m_View;
}

glm::vec3 PerspectiveCamera::GetForward() const {
    glm::vec3 front;
    front.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
    front.y = sin(glm::radians(m_Pitch));
    front.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
    return glm::normalize(front);
}

glm::vec3 PerspectiveCamera::GetRight() const {
    return glm::normalize(glm::cross(GetForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 PerspectiveCamera::GetUp() const {
    return glm::normalize(glm::cross(GetRight(), GetForward()));
}

void PerspectiveCamera::RecalculateView() {
    glm::vec3 front = GetForward();
    m_View = glm::lookAt(m_Position, m_Position + front, glm::vec3(0.0f, 1.0f, 0.0f));
    m_ViewProjection = m_Projection * m_View;
}

} // namespace Engine
