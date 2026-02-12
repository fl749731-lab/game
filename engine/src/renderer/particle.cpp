#include "engine/renderer/particle.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <random>

namespace Engine {

std::vector<Particle> ParticleSystem::s_Pool;
u32 ParticleSystem::s_AliveCount = 0;
u32 ParticleSystem::s_QuadVAO = 0;
u32 ParticleSystem::s_QuadVBO = 0;
u32 ParticleSystem::s_InstanceVBO = 0;
Ref<Shader> ParticleSystem::s_Shader = nullptr;
std::vector<ParticleSystem::InstanceData> ParticleSystem::s_InstanceBuffer;

// ── 实例化粒子着色器 ────────────────────────────────────────

static const char* pVert = R"(
#version 450 core
layout(location = 0) in vec3 aPos;

// 实例属性
layout(location = 1) in vec3 aParticlePos;
layout(location = 2) in float aSize;
layout(location = 3) in vec3 aColor;
layout(location = 4) in float aAlpha;

uniform mat4 uVP;
uniform vec3 uRight;
uniform vec3 uUp;

out vec2 vUV;
out vec3 vColor;
out float vAlpha;

void main() {
    vec3 worldPos = aParticlePos
        + uRight * aPos.x * aSize
        + uUp * aPos.y * aSize;
    gl_Position = uVP * vec4(worldPos, 1.0);
    vUV = aPos.xy * 0.5 + 0.5;
    vColor = aColor;
    vAlpha = aAlpha;
}
)";

static const char* pFrag = R"(
#version 450 core
in vec2 vUV;
in vec3 vColor;
in float vAlpha;
out vec4 FragColor;

void main() {
    // 软圆形粒子
    float dist = length(vUV - 0.5) * 2.0;
    float alpha = 1.0 - smoothstep(0.6, 1.0, dist);
    if (alpha < 0.01) discard;
    FragColor = vec4(vColor, alpha * vAlpha);
}
)";

static const float quadVerts[] = {
    -1, -1, 0,   1, -1, 0,   1, 1, 0,
    -1, -1, 0,   1,  1, 0,  -1, 1, 0,
};

static std::mt19937 s_RNG(std::random_device{}());

static float RandFloat(float lo, float hi) {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(s_RNG);
}

void ParticleSystem::Init() {
    glGenVertexArrays(1, &s_QuadVAO);
    glGenBuffers(1, &s_QuadVBO);
    glGenBuffers(1, &s_InstanceVBO);

    glBindVertexArray(s_QuadVAO);

    // 顶点数据（每个粒子共享的四边形）
    glBindBuffer(GL_ARRAY_BUFFER, s_QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);

    // 实例数据缓冲区
    glBindBuffer(GL_ARRAY_BUFFER, s_InstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, 1000 * sizeof(InstanceData), nullptr, GL_DYNAMIC_DRAW);

    // aParticlePos (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                          (void*)offsetof(InstanceData, Position));
    glVertexAttribDivisor(1, 1);

    // aSize (location 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                          (void*)offsetof(InstanceData, Size));
    glVertexAttribDivisor(2, 1);

    // aColor (location 3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                          (void*)offsetof(InstanceData, Color));
    glVertexAttribDivisor(3, 1);

    // aAlpha (location 4)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(InstanceData),
                          (void*)offsetof(InstanceData, Alpha));
    glVertexAttribDivisor(4, 1);

    glBindVertexArray(0);

    s_Shader = std::make_shared<Shader>(pVert, pFrag);
    s_Pool.resize(1000);
    s_InstanceBuffer.reserve(1000);
    LOG_INFO("[粒子] 初始化完成 - 实例化渲染 (池大小: %zu)", s_Pool.size());
}

void ParticleSystem::Shutdown() {
    if (s_QuadVAO)     { glDeleteVertexArrays(1, &s_QuadVAO);  s_QuadVAO = 0; }
    if (s_QuadVBO)     { glDeleteBuffers(1, &s_QuadVBO);       s_QuadVBO = 0; }
    if (s_InstanceVBO) { glDeleteBuffers(1, &s_InstanceVBO);   s_InstanceVBO = 0; }
    s_Shader.reset();
    s_Pool.clear();
    s_InstanceBuffer.clear();
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
    p.Gravity = cfg.Gravity;
    p.ColorStart = cfg.ColorStart;
    p.ColorEnd = cfg.ColorEnd;
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

        p.Velocity.y += p.Gravity * dt;
        p.Position += p.Velocity * dt;

        // 颜色插值
        f32 t = (p.MaxLife > 0.001f) ? (1.0f - p.Life / p.MaxLife) : 1.0f;
        p.Color = glm::mix(p.ColorStart, p.ColorEnd, t);

        s_AliveCount++;
    }
}

void ParticleSystem::Draw(const f32* viewProjectionMatrix,
                           const glm::vec3& cameraRight,
                           const glm::vec3& cameraUp) {
    if (s_AliveCount == 0) return;

    // 收集活粒子实例数据
    s_InstanceBuffer.clear();
    for (auto& p : s_Pool) {
        if (!p.IsAlive()) continue;
        InstanceData id;
        id.Position = p.Position;
        id.Size = p.Size;
        id.Color = p.Color;
        id.Alpha = (p.MaxLife > 0.001f) ? (p.Life / p.MaxLife) : 0.0f;
        s_InstanceBuffer.push_back(id);
    }

    if (s_InstanceBuffer.empty()) return;

    // 上传实例数据
    glBindBuffer(GL_ARRAY_BUFFER, s_InstanceVBO);
    size_t dataSize = s_InstanceBuffer.size() * sizeof(InstanceData);
    // 如果缓冲区不够大，重新分配
    size_t currentCapacity = s_Pool.size() * sizeof(InstanceData);
    if (dataSize > currentCapacity) {
        glBufferData(GL_ARRAY_BUFFER, dataSize, s_InstanceBuffer.data(), GL_DYNAMIC_DRAW);
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)dataSize, s_InstanceBuffer.data());
    }

    // 叠加混合
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    s_Shader->Bind();
    s_Shader->SetMat4("uVP", viewProjectionMatrix);
    s_Shader->SetVec3("uRight", cameraRight.x, cameraRight.y, cameraRight.z);
    s_Shader->SetVec3("uUp", cameraUp.x, cameraUp.y, cameraUp.z);

    // 单次实例化 DrawCall！
    glBindVertexArray(s_QuadVAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)s_InstanceBuffer.size());
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

u32 ParticleSystem::GetAliveCount() { return s_AliveCount; }

} // namespace Engine
