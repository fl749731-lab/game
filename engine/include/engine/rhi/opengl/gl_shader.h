#pragma once

#include "engine/rhi/rhi_shader.h"

#include <glad/glad.h>
#include <unordered_map>

namespace Engine {

// ── OpenGL 着色器程序 ───────────────────────────────────────

class GLShader : public RHIShader {
public:
    GLShader(const std::string& vertexSrc, const std::string& fragmentSrc);
    ~GLShader() override;

    GLShader(const GLShader&) = delete;
    GLShader& operator=(const GLShader&) = delete;

    void Bind() const override;
    void Unbind() const override;
    bool IsValid() const override { return m_Valid; }

    // Uniform 设置
    void SetInt(const std::string& name, i32 value) override;
    void SetFloat(const std::string& name, f32 value) override;
    void SetVec2(const std::string& name, f32 x, f32 y) override;
    void SetVec3(const std::string& name, f32 x, f32 y, f32 z) override;
    void SetVec4(const std::string& name, f32 x, f32 y, f32 z, f32 w) override;
    void SetMat3(const std::string& name, const f32* value) override;
    void SetMat4(const std::string& name, const f32* value) override;

    u32 GetID() const { return m_ID; }

private:
    u32  m_ID    = 0;
    bool m_Valid = false;
    mutable std::unordered_map<std::string, i32> m_UniformCache;

    u32 CompileShader(u32 type, const std::string& source);
    i32 GetUniformLocation(const std::string& name);
};

} // namespace Engine
