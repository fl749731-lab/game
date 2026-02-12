#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"

namespace Engine {

// ── Bloom 后处理效果 ─────────────────────────────────────────
// HDR 渲染 → 亮度提取 → 两遍高斯模糊（ping-pong）→ 合成
// 在 PostProcess 管线中作为前置步骤

class Bloom {
public:
    static void Init(u32 width, u32 height);
    static void Shutdown();

    /// 执行 Bloom 效果：输入 HDR 纹理，输出 Bloom 纹理
    /// @param hdrInputTexture  场景 HDR 颜色纹理 ID
    /// @return Bloom 纹理 ID（高斯模糊后的亮区）
    static u32 Process(u32 hdrInputTexture);

    /// 重新创建内部 FBO（窗口大小变化时调用）
    static void Resize(u32 width, u32 height);

    // ── 参数控制 ─────────────────────────────────────────────
    static void SetThreshold(f32 threshold);
    static f32 GetThreshold();

    static void SetIntensity(f32 intensity);
    static f32 GetIntensity();

    static void SetIterations(u32 iterations);
    static u32 GetIterations();

    static void SetEnabled(bool enabled);
    static bool IsEnabled();

    /// Bloom 纹理 ID（供 PostProcess 合成用）
    static u32 GetBloomTexture();

private:
    static void CreateFBOs(u32 width, u32 height);

    static u32 s_BrightFBO;       // 亮度提取 FBO
    static u32 s_BrightTexture;   // 亮度提取纹理

    static u32 s_PingFBO;         // Ping-Pong FBO A
    static u32 s_PingTexture;
    static u32 s_PongFBO;         // Ping-Pong FBO B
    static u32 s_PongTexture;

    static u32 s_QuadVAO;
    static u32 s_QuadVBO;

    static Ref<Shader> s_BrightShader;
    static Ref<Shader> s_BlurShader;

    static u32 s_Width;
    static u32 s_Height;

    static f32 s_Threshold;      // 亮度提取阈值（默认 1.0）
    static f32 s_Intensity;      // Bloom 混合强度（默认 0.5）
    static u32 s_Iterations;     // 模糊迭代次数（默认 5）
    static bool s_Enabled;
};

} // namespace Engine
