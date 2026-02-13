#pragma once

#include "engine/core/log.h"

#include <glad/glad.h>

namespace Engine {

// ── OpenGL 调试工具 ─────────────────────────────────────────
//
// GL_CALL(expr)  — 在 Debug 构建中自动检查每个 GL 调用后的错误
// GL_CHECK()     — 手动检查当前 GL 错误状态
//
// 注意: 引擎已在 Renderer::Init() 中启用了 glDebugMessageCallback，
// 该宏提供额外的逐行级别错误检查能力。

#ifdef ENGINE_DEBUG

inline const char* GLErrorString(GLenum err) {
    switch (err) {
        case GL_INVALID_ENUM:                  return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:                 return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:             return "GL_INVALID_OPERATION";
#ifdef GL_INVALID_FRAMEBUFFER_OPERATION
        case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
#endif
        case GL_OUT_OF_MEMORY:                 return "GL_OUT_OF_MEMORY";
#ifdef GL_STACK_UNDERFLOW
        case GL_STACK_UNDERFLOW:               return "GL_STACK_UNDERFLOW";
#endif
#ifdef GL_STACK_OVERFLOW
        case GL_STACK_OVERFLOW:                return "GL_STACK_OVERFLOW";
#endif
        default:                               return "UNKNOWN_GL_ERROR";
    }
}

inline void GLCheckError(const char* file, int line, const char* expr) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        LOG_ERROR("[GL] %s (%d) at %s:%d — 调用: %s",
                  GLErrorString(err), err, file, line, expr);
    }
}

#define GL_CALL(x) do { (x); ::Engine::GLCheckError(__FILE__, __LINE__, #x); } while(0)
#define GL_CHECK()      ::Engine::GLCheckError(__FILE__, __LINE__, "GL_CHECK()")

#else

#define GL_CALL(x) (x)
#define GL_CHECK()

#endif // ENGINE_DEBUG

} // namespace Engine
