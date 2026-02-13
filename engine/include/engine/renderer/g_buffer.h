#pragma once

#include "engine/core/types.h"
#include "engine/renderer/framebuffer.h"

#include <memory>

namespace Engine {

// ── G-Buffer (延迟渲染几何缓冲) ─────────────────────────────
//
// 附件布局:
//   RT0 (RGB16F) — 世界空间位置 (Position.xyz)
//   RT1 (RGB16F) — 世界空间法线 (Normal.xyz)
//   RT2 (RGBA8)  — 漫反射颜色 + 高光强度 (Albedo.rgb, Specular)
//   RT3 (RGBA8)  — 自发光 (Emissive.rgb, Reserved)
//   Depth (D24)  — 深度缓冲 (前向叠加 Pass 复用)

class GBuffer {
public:
    static void Init(u32 width, u32 height);
    static void Shutdown();
    static void Resize(u32 width, u32 height);

    /// 绑定 G-Buffer FBO (用于几何 Pass 写入)
    static void Bind();
    static void Unbind();

    /// 将所有 G-Buffer 纹理绑定到指定纹理单元 (用于光照 Pass 采样)
    /// unit0 = Position, unit1 = Normal, unit2 = AlbedoSpec, unit3 = Emissive
    static void BindTextures(u32 startUnit = 0);

    /// 获取各 RT 的纹理 ID
    static u32 GetPositionTexture();
    static u32 GetNormalTexture();
    static u32 GetAlbedoSpecTexture();
    static u32 GetEmissiveTexture();
    static u32 GetDepthTexture();

    /// 获取底层 FBO (用于深度缓冲 blit)
    static u32 GetFBO();
    static u32 GetWidth();
    static u32 GetHeight();

private:
    static std::unique_ptr<Framebuffer> s_FBO;
    static u32 s_Width;
    static u32 s_Height;
};

} // namespace Engine
