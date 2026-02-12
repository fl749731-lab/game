#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"

namespace Engine {

// ── 后处理管线 ──────────────────────────────────────────────
// 全屏三角形绘制 + 色调映射/伽马校正

class PostProcess {
public:
    static void Init();
    static void Shutdown();

    /// 绘制全屏四边形，采样 sourceTexture
    static void Draw(u32 sourceTextureID);

    /// 绘制全屏四边形，采样 sourceTexture 并混合 bloomTexture
    static void Draw(u32 sourceTextureID, u32 bloomTextureID, f32 bloomIntensity);

    /// 获取内置后处理 shader（外部可修改参数）
    static Ref<Shader> GetShader();

    // 后处理参数
    static void SetExposure(f32 exposure);
    static void SetGamma(f32 gamma);
    static void SetVignetteStrength(f32 strength);

private:
    static u32 s_QuadVAO;
    static u32 s_QuadVBO;
    static Ref<Shader> s_Shader;
    static f32 s_Exposure;
    static f32 s_Gamma;
    static f32 s_VignetteStrength;
};

} // namespace Engine
