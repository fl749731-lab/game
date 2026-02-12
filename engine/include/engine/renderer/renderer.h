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

    // ── 渲染统计 ────────────────────────────────────────────
    struct Stats {
        u32 DrawCalls = 0;
        u32 TriangleCount = 0;
    };

    static void ResetStats();
    static Stats GetStats();

    // ── 面剔除控制 ──────────────────────────────────────────
    static void SetCullFace(bool enabled);
    static void SetWireframe(bool enabled);

    /// 外部绘制时手动更新统计（如 Mesh::Draw 直接调用 glDrawElements）
    static void NotifyDraw(u32 triangleCount = 0);

private:
    static Stats s_Stats;
};

} // namespace Engine
