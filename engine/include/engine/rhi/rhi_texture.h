#pragma once

#include "engine/rhi/rhi_types.h"

#include <string>

namespace Engine {

// ── 抽象 2D 纹理 ────────────────────────────────────────────

class RHITexture2D {
public:
    virtual ~RHITexture2D() = default;

    /// 绑定到指定纹理槽位
    virtual void Bind(u32 slot = 0) const = 0;
    virtual void Unbind() const = 0;

    /// 更新纹理数据（子区域）
    virtual void SetData(const void* data, u32 size) = 0;

    virtual u32 GetWidth() const = 0;
    virtual u32 GetHeight() const = 0;
    virtual bool IsValid() const = 0;
};

} // namespace Engine
