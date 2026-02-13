#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"

namespace Engine {

// ── SSAO (Screen Space Ambient Occlusion) ───────────────────
//
// 基于 G-Buffer 的 Position 和 Normal 纹理，在屏幕空间计算
// 环境遮蔽因子。使用半球采样 + 噪声旋转 + 高斯模糊。
//
// 用法:
//   1. Init(width, height)
//   2. 在 LightingPass 之前调用 Generate(projection)
//   3. 在 LightingPass 中采样 GetOcclusionTexture() 作为 AO 因子
//   4. Resize() 跟随窗口尺寸

class SSAO {
public:
    static void Init(u32 width, u32 height);
    static void Shutdown();
    static void Resize(u32 width, u32 height);

    /// 生成 SSAO 纹理（需要 G-Buffer 已填充）
    static void Generate(const f32* projectionMatrix);

    /// 获取最终模糊后的 AO 纹理
    static u32 GetOcclusionTexture();

    /// SSAO 参数
    static void SetRadius(f32 radius);
    static void SetBias(f32 bias);
    static void SetIntensity(f32 intensity);
    static void SetEnabled(bool enabled);
    static bool IsEnabled();

private:
    static void CreateKernelAndNoise();

    static u32 s_SSAO_FBO;      // SSAO 原始输出
    static u32 s_SSAO_Texture;
    static u32 s_Blur_FBO;      // 模糊后输出
    static u32 s_Blur_Texture;
    static u32 s_NoiseTex;      // 4x4 随机噪声纹理

    static Ref<Shader> s_SSAOShader;
    static Ref<Shader> s_BlurShader;

    static u32 s_Width, s_Height;
    static f32 s_Radius;
    static f32 s_Bias;
    static f32 s_Intensity;
    static bool s_Enabled;
};

} // namespace Engine
