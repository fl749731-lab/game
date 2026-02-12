#pragma once

#include "engine/core/types.h"
#include <string>
#include <unordered_map>

namespace Engine {

// ── Shader 程序 ─────────────────────────────────────────────

class Shader {
public:
    Shader(const std::string& vertexSrc, const std::string& fragmentSrc);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void Bind() const;
    void Unbind() const;

    u32 GetID() const { return m_ID; }
    bool IsValid() const { return m_Valid; }

    // Uniform 设置
    void SetInt(const std::string& name, i32 value);
    void SetFloat(const std::string& name, f32 value);
    void SetVec2(const std::string& name, f32 x, f32 y);
    void SetVec3(const std::string& name, f32 x, f32 y, f32 z);
    void SetVec4(const std::string& name, f32 x, f32 y, f32 z, f32 w);
    void SetMat4(const std::string& name, const f32* value);
    void SetMat3(const std::string& name, const f32* value);

private:
    u32 m_ID = 0;
    bool m_Valid = false;
    mutable std::unordered_map<std::string, i32> m_UniformCache;

    u32 CompileShader(u32 type, const std::string& source);
    i32 GetUniformLocation(const std::string& name);
};

} // namespace Engine
