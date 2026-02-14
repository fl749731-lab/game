#pragma once

#include "engine/rhi/rhi_types.h"

namespace Engine {

// ── 抽象帧缓冲 ──────────────────────────────────────────────

class RHIFramebuffer {
public:
    virtual ~RHIFramebuffer() = default;

    virtual void Bind() const = 0;
    virtual void Unbind() const = 0;
    virtual void Resize(u32 width, u32 height) = 0;

    virtual u32 GetColorAttachmentCount() const = 0;
    virtual u32 GetWidth() const = 0;
    virtual u32 GetHeight() const = 0;
    virtual bool IsValid() const = 0;
};

} // namespace Engine
