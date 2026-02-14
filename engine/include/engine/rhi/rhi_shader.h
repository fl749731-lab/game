#pragma once

#include "engine/rhi/rhi_types.h"

#include <string>

namespace Engine {

// ── 抽象着色器程序 ──────────────────────────────────────────

class RHIShader {
public:
    virtual ~RHIShader() = default;

    virtual void Bind() const = 0;
    virtual void Unbind() const = 0;

    virtual bool IsValid() const = 0;

    // ── Uniform 设置 ────────────────────────────────────────
    virtual void SetInt(const std::string& name, i32 value) = 0;
    virtual void SetFloat(const std::string& name, f32 value) = 0;
    virtual void SetVec2(const std::string& name, f32 x, f32 y) = 0;
    virtual void SetVec3(const std::string& name, f32 x, f32 y, f32 z) = 0;
    virtual void SetVec4(const std::string& name, f32 x, f32 y, f32 z, f32 w) = 0;
    virtual void SetMat3(const std::string& name, const f32* value) = 0;
    virtual void SetMat4(const std::string& name, const f32* value) = 0;
};

} // namespace Engine
