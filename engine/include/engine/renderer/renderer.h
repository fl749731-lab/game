#pragma once

#include "engine/core/types.h"

namespace Engine {

// ── 渲染器 ──────────────────────────────────────────────────

class Renderer {
public:
    static void Init();
    static void Shutdown();

    static void SetClearColor(f32 r, f32 g, f32 b, f32 a = 1.0f);
    static void Clear();

    static void SetViewport(u32 x, u32 y, u32 width, u32 height);
    
    static void DrawArrays(u32 vao, u32 vertexCount);
    static void DrawElements(u32 vao, u32 indexCount);
};

} // namespace Engine
