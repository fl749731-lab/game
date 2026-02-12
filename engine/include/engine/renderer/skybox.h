#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"

namespace Engine {

// ── 渐变天空盒 ──────────────────────────────────────────────
// 程序化天空（顶/底/地平线三色渐变），无需立方体贴图

class Skybox {
public:
    static void Init();
    static void Shutdown();

    /// 渲染天空盒（在场景物体之前调用，写入深度最远值）
    static void Draw(const f32* viewProjectionMatrix);

    // 颜色设置
    static void SetTopColor(f32 r, f32 g, f32 b);
    static void SetHorizonColor(f32 r, f32 g, f32 b);
    static void SetBottomColor(f32 r, f32 g, f32 b);
    static void SetSunDirection(f32 x, f32 y, f32 z);

private:
    static u32 s_CubeVAO;
    static u32 s_CubeVBO;
    static Ref<Shader> s_Shader;
    static f32 s_TopColor[3];
    static f32 s_HorizonColor[3];
    static f32 s_BottomColor[3];
    static f32 s_SunDir[3];
};

} // namespace Engine
