#pragma once

#include "engine/core/types.h"

namespace Engine {

// ── 屏幕空间反射 (SSR) ──────────────────────────────────────
//
// 基于屏幕空间光线步进 (Ray Marching) 实现反射效果。
// 利用 G-Buffer 中的位置、法线、Roughness 等信息，
// 在屏幕空间中追踪反射射线，采样场景颜色作为反射结果。
//
// 集成方式:
//   1. SSR::Init() 在 SceneRenderer::Init 中调用
//   2. SSR::Generate() 在 LightingPass 之后、PostProcess 之前调用
//   3. 将 SSR 结果混合到 HDR 缓冲区

class SSR {
public:
    static void Init(u32 width, u32 height);
    static void Shutdown();
    static void Resize(u32 width, u32 height);

    /// 生成 SSR 纹理
    /// @param projMatrix  投影矩阵 (glm::value_ptr)
    /// @param viewMatrix  视图矩阵 (glm::value_ptr)
    /// @param hdrTexture  当前场景 HDR 颜色纹理
    static void Generate(const f32* projMatrix, const f32* viewMatrix,
                         u32 hdrTexture);

    /// 获取 SSR 结果纹理
    static u32 GetReflectionTexture();

    /// 开关
    static bool IsEnabled();
    static void SetEnabled(bool enabled);

    /// 参数控制
    static void SetMaxSteps(i32 steps);
    static void SetStepSize(f32 size);
    static void SetThickness(f32 thickness);

private:
    static u32 s_FBO;
    static u32 s_ReflectionTex;
    static u32 s_Width, s_Height;
    static bool s_Enabled;
    static i32 s_MaxSteps;
    static f32 s_StepSize;
    static f32 s_Thickness;
};

} // namespace Engine
