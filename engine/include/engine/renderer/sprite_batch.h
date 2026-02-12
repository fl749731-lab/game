#pragma once

#include "engine/core/types.h"
#include "engine/renderer/texture.h"
#include "engine/renderer/shader.h"

#include <glm/glm.hpp>

namespace Engine {

// ── 2D 精灵批渲染器 ────────────────────────────────────────
// 批量绘制 2D 四边形，使用正交投影
// 支持纹理/纯色/旋转/缩放

class SpriteBatch {
public:
    /// 初始化渲染资源
    static void Init();

    /// 清理
    static void Shutdown();

    /// 开始一帧的 2D 渲染 (设置正交投影)
    static void Begin(u32 screenWidth, u32 screenHeight);

    /// 绘制纹理精灵
    static void Draw(Ref<Texture2D> texture,
                     const glm::vec2& position,
                     const glm::vec2& size,
                     f32 rotation = 0.0f,
                     const glm::vec4& tint = glm::vec4(1.0f));

    /// 绘制纯色矩形
    static void DrawRect(const glm::vec2& position,
                         const glm::vec2& size,
                         const glm::vec4& color,
                         f32 rotation = 0.0f);

    /// 结束当前帧 (Flush 到 GPU)
    static void End();

    /// 渲染统计
    static u32 GetDrawCallCount();
    static u32 GetQuadCount();

    /// 常量
    static constexpr u32 MAX_QUADS = 1000;
    static constexpr u32 MAX_VERTICES = MAX_QUADS * 4;
    static constexpr u32 MAX_INDICES = MAX_QUADS * 6;
    static constexpr u32 MAX_TEXTURE_SLOTS = 16;

private:
    static void Flush();
    static void StartBatch();
};

} // namespace Engine
