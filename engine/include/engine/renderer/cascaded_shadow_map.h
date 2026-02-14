#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/light.h"
#include "engine/renderer/camera.h"

#include <glm/glm.hpp>

#include <array>

namespace Engine {

// ── 级联阴影映射 (CSM) ──────────────────────────────────────
// 4 级联方向光阴影 — 每级联独立深度 FBO
// Log-Uniform 混合分割视锥
// PCF 3×3 软阴影 + 级联间平滑过渡
//
// 替代旧的单级 ShadowMap，提供远距离高质量阴影。

static constexpr u32 CSM_CASCADE_COUNT = 4;

struct CSMConfig {
    u32  Resolution       = 2048;    // 每级联分辨率
    f32  SplitLambda      = 0.75f;   // Log-Uniform 混合因子 (0=uniform, 1=log)
    f32  ShadowDistance   = 100.0f;  // 最远阴影距离
    f32  CascadeOverlap   = 0.1f;    // 级联重叠比例 (用于平滑过渡)
};

class CascadedShadowMap {
public:
    static void Init(const CSMConfig& config = {});
    static void Shutdown();

    /// 更新级联分割平面 (每帧在 ShadowPass 前调用)
    static void UpdateCascades(const PerspectiveCamera& camera,
                               const DirectionalLight& light);

    /// 开始指定级联的深度 Pass
    static void BeginCascadePass(u32 cascadeIndex);

    /// 结束当前级联深度 Pass
    static void EndCascadePass();

    /// 获取深度着色器
    static Ref<Shader> GetDepthShader();

    /// 绑定所有级联阴影纹理到指定起始纹理单元
    /// 返回下一个可用纹理单元
    static u32 BindCascadeTextures(u32 startUnit);

    /// 设置 CSM Uniform 到 Shader
    static void SetUniforms(Shader* shader, u32 startUnit);

    /// 获取级联分割距离数组
    static const std::array<f32, CSM_CASCADE_COUNT + 1>& GetSplitDistances();

    /// 获取级联 Light Space 矩阵
    static const std::array<glm::mat4, CSM_CASCADE_COUNT>& GetLightSpaceMatrices();

    /// 获取分辨率
    static u32 GetResolution();

    /// 获取配置（可修改）
    static CSMConfig& GetConfig();

private:
    // 每级联独立 FBO + 深度纹理
    static std::array<u32, CSM_CASCADE_COUNT> s_FBOs;
    static std::array<u32, CSM_CASCADE_COUNT> s_DepthTextures;

    // 级联分割
    static std::array<f32, CSM_CASCADE_COUNT + 1> s_SplitDistances;
    static std::array<glm::mat4, CSM_CASCADE_COUNT> s_LightSpaceMatrices;

    static u32 s_Resolution;
    static Ref<Shader> s_DepthShader;
    static CSMConfig s_Config;

    /// 计算级联分割距离 (Log-Uniform 混合)
    static void CalculateSplits(f32 nearPlane, f32 farPlane);

    /// 计算指定级联的 Light Space 矩阵
    static glm::mat4 CalculateLightSpaceMatrix(
        const PerspectiveCamera& camera,
        const DirectionalLight& light,
        f32 nearSplit, f32 farSplit);
};

} // namespace Engine
