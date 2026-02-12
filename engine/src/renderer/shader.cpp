#include "engine/renderer/shader.h"
#include "engine/core/log.h"

#include <glad/glad.h>

namespace Engine {

Shader::Shader(const std::string& vertexSrc, const std::string& fragmentSrc) {
    u32 vertShader = CompileShader(GL_VERTEX_SHADER, vertexSrc);
    u32 fragShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);

    m_ID = glCreateProgram();
    glAttachShader(m_ID, vertShader);
    glAttachShader(m_ID, fragShader);
    glLinkProgram(m_ID);

    GLint success;
    glGetProgramiv(m_ID, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_ID, 512, nullptr, infoLog);
        LOG_ERROR("Shader 程序链接失败: %s", infoLog);
        m_Valid = false;
    } else {
        LOG_DEBUG("Shader 程序 %u 链接成功", m_ID);
        m_Valid = true;
    }

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
}

Shader::~Shader() {
    if (m_ID) {
        glDeleteProgram(m_ID);
    }
}

void Shader::Bind() const {
    if (!m_Valid) return;
    glUseProgram(m_ID);
}

void Shader::Unbind() const {
    glUseProgram(0);
}

u32 Shader::CompileShader(u32 type, const std::string& source) {
    u32 shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        const char* typeStr = (type == GL_VERTEX_SHADER) ? "顶点" : "片段";
        LOG_ERROR("%s着色器编译失败: %s", typeStr, infoLog);
    }

    return shader;
}

i32 Shader::GetUniformLocation(const std::string& name) {
    if (!m_Valid) return -1;
    auto it = m_UniformCache.find(name);
    if (it != m_UniformCache.end()) return it->second;

    i32 loc = glGetUniformLocation(m_ID, name.c_str());
    m_UniformCache[name] = loc;
    return loc;
}

void Shader::SetInt(const std::string& name, i32 value) {
    glUniform1i(GetUniformLocation(name), value);
}

void Shader::SetFloat(const std::string& name, f32 value) {
    glUniform1f(GetUniformLocation(name), value);
}

void Shader::SetVec2(const std::string& name, f32 x, f32 y) {
    glUniform2f(GetUniformLocation(name), x, y);
}

void Shader::SetVec3(const std::string& name, f32 x, f32 y, f32 z) {
    glUniform3f(GetUniformLocation(name), x, y, z);
}

void Shader::SetVec4(const std::string& name, f32 x, f32 y, f32 z, f32 w) {
    glUniform4f(GetUniformLocation(name), x, y, z, w);
}

void Shader::SetMat4(const std::string& name, const f32* value) {
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, value);
}

void Shader::SetMat3(const std::string& name, const f32* value) {
    glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, value);
}

} // namespace Engine
