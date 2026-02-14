#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"

namespace Engine {

// ── Image-Based Lighting (IBL) ─────────────────────────────
// 提供基于图像的全局照明:
//   1. BRDF 积分 LUT (预计算)
//   2. 辐照度图 (Irradiance Map) — 漫反射环境光
//   3. 预滤波环境图 (Prefilter Map) — 镜面反射

class IBL {
public:
    /// 初始化 IBL 系统 (生成 BRDF LUT)
    static void Init();

    /// 从 HDR 等距柱状投影图加载环境贴图并预计算 IBL 贴图
    static void LoadEnvironmentMap(const char* hdrPath);

    /// 从已有的立方体贴图 ID 预计算 IBL
    static void ComputeFromCubemap(u32 envCubemap);

    /// 关闭并释放资源
    static void Shutdown();

    /// 绑定 IBL 纹理到指定纹理单元
    /// 默认: irradiance=10, prefilter=11, brdfLUT=12
    static void Bind(u32 irradianceUnit = 10,
                     u32 prefilterUnit = 11,
                     u32 brdfLUTUnit = 12);

    /// 设置 IBL shader uniforms
    static void SetUniforms(Shader* shader,
                            u32 irradianceUnit = 10,
                            u32 prefilterUnit = 11,
                            u32 brdfLUTUnit = 12);

    static u32 GetIrradianceMap() { return s_IrradianceMap; }
    static u32 GetPrefilterMap()  { return s_PrefilterMap; }
    static u32 GetBRDF_LUT()     { return s_BRDF_LUT; }
    static u32 GetEnvCubemap()   { return s_EnvCubemap; }

    static bool IsReady() { return s_Ready; }

    /// IBL 强度
    static void SetIntensity(f32 intensity) { s_Intensity = intensity; }
    static f32  GetIntensity() { return s_Intensity; }

private:
    /// 生成 BRDF 积分 LUT
    static void GenerateBRDF_LUT();

    /// 等距柱状投影 → 立方体贴图
    static u32 EquirectToCubemap(u32 hdrTexture);

    /// 生成辐照度卷积立方体贴图
    static void GenerateIrradianceMap(u32 envCubemap);

    /// 生成预滤波环境贴图 (多 mip 级别)
    static void GeneratePrefilterMap(u32 envCubemap);

    static u32 s_EnvCubemap;      // 环境立方体贴图
    static u32 s_IrradianceMap;   // 辐照度图 (32x32)
    static u32 s_PrefilterMap;    // 预滤波图 (128x128, 5 mip)
    static u32 s_BRDF_LUT;        // BRDF LUT (512x512)

    static u32 s_CaptureFBO;      // 用于离屏渲染的 FBO
    static u32 s_CaptureRBO;

    static Ref<Shader> s_EquirectShader;
    static Ref<Shader> s_IrradianceShader;
    static Ref<Shader> s_PrefilterShader;
    static Ref<Shader> s_BRDFShader;

    static f32  s_Intensity;
    static bool s_Ready;
};

} // namespace Engine
