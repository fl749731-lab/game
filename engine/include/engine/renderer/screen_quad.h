#pragma once

#include "engine/core/types.h"

namespace Engine {

// ── 共享全屏四边形 ──────────────────────────────────────────
// Bloom 和 PostProcess 共用的全屏四边形 VAO/VBO
// 避免重复创建相同的 GPU 资源

class ScreenQuad {
public:
    static void Init();
    static void Shutdown();

    /// 绑定 VAO 并绘制全屏四边形（6 顶点）
    static void Draw();

    static u32 GetVAO() { return s_VAO; }

private:
    static u32 s_VAO;
    static u32 s_VBO;
    static bool s_Initialized;
};

} // namespace Engine
