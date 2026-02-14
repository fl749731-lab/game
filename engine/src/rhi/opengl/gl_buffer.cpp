#include "engine/rhi/opengl/gl_buffer.h"
#include "engine/core/log.h"

// 自定义 GLAD 可能缺少此常量
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW 0x88E0
#endif

namespace Engine {

// ── GLVertexBuffer ──────────────────────────────────────────

static GLenum ToGLUsage(RHIBufferUsage usage) {
    switch (usage) {
        case RHIBufferUsage::Static:  return GL_STATIC_DRAW;
        case RHIBufferUsage::Dynamic: return GL_DYNAMIC_DRAW;
        case RHIBufferUsage::Stream:  return GL_STREAM_DRAW;
    }
    return GL_STATIC_DRAW;
}

GLVertexBuffer::GLVertexBuffer(const void* data, u32 size, RHIBufferUsage usage)
    : m_Size(size)
{
    glGenBuffers(1, &m_ID);
    glBindBuffer(GL_ARRAY_BUFFER, m_ID);
    glBufferData(GL_ARRAY_BUFFER, size, data, ToGLUsage(usage));
}

GLVertexBuffer::~GLVertexBuffer() {
    if (m_ID) glDeleteBuffers(1, &m_ID);
}

void GLVertexBuffer::Bind() const {
    glBindBuffer(GL_ARRAY_BUFFER, m_ID);
}

void GLVertexBuffer::Unbind() const {
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GLVertexBuffer::SetData(const void* data, u32 size) {
    glBindBuffer(GL_ARRAY_BUFFER, m_ID);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
    m_Size = size;
}

// ── GLIndexBuffer ───────────────────────────────────────────

GLIndexBuffer::GLIndexBuffer(const u32* indices, u32 count)
    : m_Count(count)
{
    glGenBuffers(1, &m_ID);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(u32), indices, GL_STATIC_DRAW);
}

GLIndexBuffer::~GLIndexBuffer() {
    if (m_ID) glDeleteBuffers(1, &m_ID);
}

void GLIndexBuffer::Bind() const {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ID);
}

void GLIndexBuffer::Unbind() const {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

// ── GLVertexArray ───────────────────────────────────────────

GLVertexArray::GLVertexArray() {
    glGenVertexArrays(1, &m_ID);
}

GLVertexArray::~GLVertexArray() {
    if (m_ID) glDeleteVertexArrays(1, &m_ID);
}

void GLVertexArray::Bind() const {
    glBindVertexArray(m_ID);
}

void GLVertexArray::Unbind() const {
    glBindVertexArray(0);
}

void GLVertexArray::AddAttribute(u32 index, i32 size, i32 stride, u64 offset) {
    glEnableVertexAttribArray(index);
    glVertexAttribPointer(index, size, GL_FLOAT, GL_FALSE, stride, (const void*)offset);
}

} // namespace Engine
