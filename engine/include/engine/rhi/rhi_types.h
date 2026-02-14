#pragma once

#include "engine/core/types.h"

namespace Engine {

// ── 图形后端枚举 ────────────────────────────────────────────

enum class GraphicsBackend : u8 {
    OpenGL,
    Vulkan
};

// ── 纹理格式 (RHI 级) ──────────────────────────────────────

enum class RHITextureFormat : u8 {
    RGBA8,       // 标准 LDR
    RGBA16F,     // HDR 浮点
    RGB16F,      // HDR 浮点 (无 Alpha)
    RG16F,       // 2 通道浮点
    R32F,        // 单通道浮点
    Depth24,     // 深度 24 位
};

// ── 缓冲用途 ────────────────────────────────────────────────

enum class RHIBufferUsage : u8 {
    Static,      // GPU-only, 创建后不可修改
    Dynamic,     // CPU→GPU 频繁更新
    Stream       // 每帧重写
};

// ── 着色器阶段 ──────────────────────────────────────────────

enum class RHIShaderStage : u8 {
    Vertex,
    Fragment,
    Compute
};

// ── 面剔除模式 ──────────────────────────────────────────────

enum class RHICullMode : u8 {
    None,
    Front,
    Back
};

// ── 深度比较函数 ────────────────────────────────────────────

enum class RHIDepthFunc : u8 {
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    NotEqual,
    Always,
    Never
};

// ── 混合因子 ────────────────────────────────────────────────

enum class RHIBlendFactor : u8 {
    Zero,
    One,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
    SrcColor,
    OneMinusSrcColor,
};

// ── 管线状态描述 ────────────────────────────────────────────

struct RHIPipelineStateDesc {
    bool DepthTest    = true;
    bool DepthWrite   = true;
    RHIDepthFunc DepthFunc = RHIDepthFunc::Less;
    bool Blending     = false;
    RHIBlendFactor SrcFactor = RHIBlendFactor::SrcAlpha;
    RHIBlendFactor DstFactor = RHIBlendFactor::OneMinusSrcAlpha;
    RHICullMode CullMode = RHICullMode::Back;
    bool Wireframe    = false;
};

// ── 帧缓冲规格 (RHI 级) ────────────────────────────────────

struct RHIFramebufferSpec {
    u32 Width  = 1280;
    u32 Height = 720;
    std::vector<RHITextureFormat> ColorFormats;
    bool DepthAttachment = true;
    bool HDR = false;
};

} // namespace Engine
