#include "engine/rhi/opengl/gl_shader.h"
#include "engine/core/log.h"

namespace Engine {

GLShader::GLShader(const std::string& vertexSrc, const std::string& fragmentSrc) {
    u32 vertShader = CompileShader(GL_VERTEX_SHADER, vertexSrc);
    u32 fragShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);

    if (vertShader == 0 || fragShader == 0) {
        if (vertShader) glDeleteShader(vertShader);
        if (fragShader) glDeleteShader(fragShader);
        m_Valid = false;
        return;
    }

    m_ID = glCreateProgram();
    glAttachShader(m_ID, vertShader);
    glAttachShader(m_ID, fragShader);
    glLinkProgram(m_ID);

    GLint success;
    glGetProgramiv(m_ID, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_ID, 512, nullptr, infoLog);
        LOG_ERROR("[GLShader] 程序链接失败: %s", infoLog);
        glDeleteProgram(m_ID);
        m_ID = 0;
        m_Valid = false;
    } else {
        LOG_DEBUG("[GLShader] 程序 %u 链接成功", m_ID);
        m_Valid = true;
    }

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
}

GLShader::~GLShader() {
    if (m_ID) glDeleteProgram(m_ID);
}

void GLShader::Bind() const {
    if (!m_Valid) return;
    glUseProgram(m_ID);
}

void GLShader::Unbind() const {
    glUseProgram(0);
}

u32 GLShader::CompileShader(u32 type, const std::string& source) {
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
        LOG_ERROR("[GLShader] %s着色器编译失败: %s", typeStr, infoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

i32 GLShader::GetUniformLocation(const std::string& name) {
    if (!m_Valid) return -1;
    auto it = m_UniformCache.find(name);
    if (it != m_UniformCache.end()) return it->second;

    i32 loc = glGetUniformLocation(m_ID, name.c_str());
    m_UniformCache[name] = loc;
    return loc;
}

void GLShader::SetInt(const std::string& name, i32 value) {
    glUniform1i(GetUniformLocation(name), value);
}

void GLShader::SetFloat(const std::string& name, f32 value) {
    glUniform1f(GetUniformLocation(name), value);
}

void GLShader::SetVec2(const std::string& name, f32 x, f32 y) {
    glUniform2f(GetUniformLocation(name), x, y);
}

void GLShader::SetVec3(const std::string& name, f32 x, f32 y, f32 z) {
    glUniform3f(GetUniformLocation(name), x, y, z);
}

void GLShader::SetVec4(const std::string& name, f32 x, f32 y, f32 z, f32 w) {
    glUniform4f(GetUniformLocation(name), x, y, z, w);
}

void GLShader::SetMat3(const std::string& name, const f32* value) {
    glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, value);
}

void GLShader::SetMat4(const std::string& name, const f32* value) {
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, value);
}

} // namespace Engine
