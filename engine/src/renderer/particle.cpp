#include "engine/renderer/particle.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace Engine {

std::vector<Particle> ParticleSystem::s_Pool;
u32 ParticleSystem::s_AliveCount = 0;
u32 ParticleSystem::s_QuadVAO = 0;
u32 ParticleSystem::s_QuadVBO = 0;
Ref<Shader> ParticleSystem::s_Shader = nullptr;

static const char* pVert = R"(
#version 450 core
layout(location = 0) in vec3 aPos;

uniform mat4 uVP;
uniform vec3 uParticlePos;
uniform float uSize;
uniform vec3 uRight;
uniform vec3 uUp;

out vec2 vUV;

void main() {
    vec3 worldPos = uParticlePos
        + uRight * aPos.x * uSize
        + uUp * aPos.y * uSize;
    gl_Position = uVP * vec4(worldPos, 1.0);
    vUV = aPos.xy * 0.5 + 0.5;
}
)";

static const char* pFrag = R"(
#version 450 core
in vec2 vUV;
out vec4 FragColor;

uniform vec3 uColor;
uniform float uAlpha;

void main() {
    // 软圆形粒子
    float dist = length(vUV - 0.5) * 2.0;
    float alpha = 1.0 - smoothstep(0.6, 1.0, dist);
    if (alpha < 0.01) discard;
    FragColor = vec4(uColor, alpha * uAlpha);
}
)";

static const float quadVerts[] = {
    -1, -1, 0,   1, -1, 0,   1, 1, 0,
    -1, -1, 0,   1,  1, 0,  -1, 1, 0,
};

static float RandFloat(float lo, float hi) {
    return lo + (float)rand() / (float)RAND_MAX * (hi - lo);
}

void ParticleSystem::Init() {
    glGenVertexArrays(1, &s_QuadVAO);
    glGenBuffers(1, &s_QuadVBO);
    glBindVertexArray(s_QuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, s_QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glBindVertexArray(0);

    s_Shader = std::make_shared<Shader>(pVert, pFrag);
    s_Pool.resize(1000); // 预分配
    LOG_INFO("[粒子] 初始化完成 (池大小: %zu)", s_Pool.size());
}

void ParticleSystem::Shutdown() {
    if (s_QuadVAO) { glDeleteVertexArrays(1, &s_QuadVAO); s_QuadVAO = 0; }
    if (s_QuadVBO) { glDeleteBuffers(1, &s_QuadVBO); s_QuadVBO = 0; }
    s_Shader.reset();
    s_Pool.clear();
}

void ParticleSystem::RespawnParticle(Particle& p, const ParticleEmitterConfig& cfg) {
    p.Position = cfg.Position;
    p.Life = RandFloat(cfg.MinLife, cfg.MaxLife);
    p.MaxLife = p.Life;
    p.Size = RandFloat(cfg.MinSize, cfg.MaxSize);
    p.Color = cfg.ColorStart;

    // 随机方向 (在锥形范围内)
    float spreadRad = cfg.SpreadAngle * 3.14159f / 180.0f;
    float theta = RandFloat(0, 6.28318f);
    float phi   = RandFloat(0, spreadRad);
    float sp = sinf(phi);
    glm::vec3 randDir(sp * cosf(theta), cosf(phi), sp * sinf(theta));

    // 校正方向
    glm::vec3 dir = glm::normalize(cfg.Direction);
    if (glm::abs(glm::dot(dir, glm::vec3(0,1,0))) < 0.99f) {
        glm::vec3 right = glm::normalize(glm::cross(dir, glm::vec3(0,1,0)));
        glm::vec3 up = glm::cross(right, dir);
        p.Velocity = (dir * randDir.y + right * randDir.x + up * randDir.z);
    } else {
        p.Velocity = randDir;
    }
    p.Velocity *= RandFloat(cfg.MinSpeed, cfg.MaxSpeed);
}

void ParticleSystem::Emit(const ParticleEmitterConfig& config, f32 dt) {
    u32 toEmit = (u32)(config.EmitRate * dt);
    if (toEmit < 1 && RandFloat(0, 1) < config.EmitRate * dt) toEmit = 1;

    u32 emitted = 0;
    for (auto& p : s_Pool) {
        if (emitted >= toEmit) break;
        if (!p.IsAlive()) {
            RespawnParticle(p, config);
            emitted++;
        }
    }
}

void ParticleSystem::Update(f32 dt) {
    s_AliveCount = 0;
    for (auto& p : s_Pool) {
        if (!p.IsAlive()) continue;
        p.Life -= dt;
        if (p.Life <= 0) continue;

        p.Velocity.y += -2.0f * dt; // 重力
        p.Position += p.Velocity * dt;

        // 颜色插值
        f32 t = 1.0f - p.Life / p.MaxLife;
        p.Color = glm::mix(glm::vec3(1, 0.8f, 0.3f), glm::vec3(1, 0.1f, 0), t);

        s_AliveCount++;
    }
}

void ParticleSystem::Draw(const f32* viewProjectionMatrix,
                           const glm::vec3& cameraRight,
                           const glm::vec3& cameraUp) {
    if (s_AliveCount == 0) return;

    // 叠加混合
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // 叠加
    glDepthMask(GL_FALSE);

    s_Shader->Bind();
    s_Shader->SetMat4("uVP", viewProjectionMatrix);
    s_Shader->SetVec3("uRight", cameraRight.x, cameraRight.y, cameraRight.z);
    s_Shader->SetVec3("uUp", cameraUp.x, cameraUp.y, cameraUp.z);

    glBindVertexArray(s_QuadVAO);
    for (auto& p : s_Pool) {
        if (!p.IsAlive()) continue;
        f32 alpha = p.Life / p.MaxLife;
        s_Shader->SetVec3("uParticlePos", p.Position.x, p.Position.y, p.Position.z);
        s_Shader->SetFloat("uSize", p.Size);
        s_Shader->SetVec3("uColor", p.Color.x, p.Color.y, p.Color.z);
        s_Shader->SetFloat("uAlpha", alpha);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // 恢复
}

u32 ParticleSystem::GetAliveCount() { return s_AliveCount; }

} // namespace Engine
