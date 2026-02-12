#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"

#include <glm/glm.hpp>
#include <vector>

namespace Engine {

// ── 粒子 ────────────────────────────────────────────────────

struct Particle {
    glm::vec3 Position = {0, 0, 0};
    glm::vec3 Velocity = {0, 0, 0};
    glm::vec3 Color    = {1, 1, 1};
    f32 Size     = 0.1f;
    f32 Life     = 1.0f;
    f32 MaxLife  = 1.0f;

    bool IsAlive() const { return Life > 0.0f; }
};

// ── 粒子发射器配置 ──────────────────────────────────────────

struct ParticleEmitterConfig {
    glm::vec3 Position = {0, 0, 0};
    glm::vec3 Direction = {0, 1, 0};     // 发射方向
    f32 SpreadAngle  = 30.0f;            // 扩散角度(度)
    f32 MinSpeed     = 1.0f;
    f32 MaxSpeed     = 3.0f;
    f32 MinLife      = 0.5f;
    f32 MaxLife      = 2.0f;
    f32 MinSize      = 0.05f;
    f32 MaxSize      = 0.15f;
    glm::vec3 ColorStart = {1, 0.8f, 0.3f};
    glm::vec3 ColorEnd   = {1, 0.1f, 0.0f};
    f32 Gravity      = -2.0f;            // 重力
    u32 EmitRate     = 50;               // 每秒粒子数
    u32 MaxParticles = 500;
};

// ── 粒子系统 ────────────────────────────────────────────────

class ParticleSystem {
public:
    static void Init();
    static void Shutdown();

    /// 发射粒子
    static void Emit(const ParticleEmitterConfig& config, f32 dt);

    /// 更新所有粒子
    static void Update(f32 dt);

    /// 渲染所有存活粒子（叠加混合，面向摄像机）
    static void Draw(const f32* viewProjectionMatrix, const glm::vec3& cameraRight,
                     const glm::vec3& cameraUp);

    /// 统计
    static u32 GetAliveCount();

private:
    static void RespawnParticle(Particle& p, const ParticleEmitterConfig& cfg);

    static std::vector<Particle> s_Pool;
    static u32 s_AliveCount;
    static u32 s_QuadVAO;
    static u32 s_QuadVBO;
    static Ref<Shader> s_Shader;
};

} // namespace Engine
