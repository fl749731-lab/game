#pragma once

#include "engine/core/types.h"
#include <glm/glm.hpp>

namespace Engine {

// ── 材质 ────────────────────────────────────────────────────

struct Material {
    glm::vec3 Ambient  = { 0.2f, 0.2f, 0.2f };
    glm::vec3 Diffuse  = { 0.8f, 0.8f, 0.8f };
    glm::vec3 Specular = { 1.0f, 1.0f, 1.0f };
    f32 Shininess = 32.0f;
};

// ── 方向光 ──────────────────────────────────────────────────

struct DirectionalLight {
    glm::vec3 Direction = { -0.3f, -1.0f, -0.5f };
    glm::vec3 Color     = { 1.0f, 0.95f, 0.9f };
    f32 Intensity = 1.0f;
};

// ── 点光源 ──────────────────────────────────────────────────

struct PointLight {
    glm::vec3 Position = { 0.0f, 2.0f, 0.0f };
    glm::vec3 Color    = { 1.0f, 1.0f, 1.0f };
    f32 Intensity = 1.0f;

    // 衰减参数
    f32 Constant  = 1.0f;
    f32 Linear    = 0.09f;
    f32 Quadratic = 0.032f;
};

// ── 最大点光源数量 ──────────────────────────────────────────
constexpr int MAX_POINT_LIGHTS = 8;

// ── 聚光灯 ──────────────────────────────────────────────────

struct SpotLight {
    glm::vec3 Position  = { 0.0f, 5.0f, 0.0f };
    glm::vec3 Direction = { 0.0f, -1.0f, 0.0f };
    glm::vec3 Color     = { 1.0f, 1.0f, 1.0f };
    f32 Intensity = 1.0f;

    f32 InnerCutoff = 12.5f;   // 内切角 (度)
    f32 OuterCutoff = 17.5f;   // 外切角 (度)

    // 衰减参数
    f32 Constant  = 1.0f;
    f32 Linear    = 0.09f;
    f32 Quadratic = 0.032f;
};

constexpr int MAX_SPOT_LIGHTS = 4;

} // namespace Engine
