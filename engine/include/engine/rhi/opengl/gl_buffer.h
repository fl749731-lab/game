#pragma once

#include "engine/rhi/rhi_buffer.h"

#include <glad/glad.h>

namespace Engine {

// ── OpenGL 顶点缓冲 ────────────────────────────────────────

class GLVertexBuffer : public RHIVertexBuffer {
public:
    GLVertexBuffer(const void* data, u32 size, RHIBufferUsage usage = RHIBufferUsage::Static);
    ~GLVertexBuffer() override;

    void Bind() const override;
    void Unbind() const override;
    void SetData(const void* data, u32 size) override;
    u32  GetSize() const override { return m_Size; }

    u32 GetID() const { return m_ID; }

private:
    u32 m_ID   = 0;
    u32 m_Size = 0;
};

// ── OpenGL 索引缓冲 ────────────────────────────────────────

class GLIndexBuffer : public RHIIndexBuffer {
public:
    GLIndexBuffer(const u32* indices, u32 count);
    ~GLIndexBuffer() override;

    void Bind() const override;
    void Unbind() const override;
    u32  GetCount() const override { return m_Count; }

    u32 GetID() const { return m_ID; }

private:
    u32 m_ID    = 0;
    u32 m_Count = 0;
};

// ── OpenGL 顶点数组 ────────────────────────────────────────

class GLVertexArray : public RHIVertexArray {
public:
    GLVertexArray();
    ~GLVertexArray() override;

    void Bind() const override;
    void Unbind() const override;
    void AddAttribute(u32 index, i32 size, i32 stride, u64 offset) override;

    u32 GetID() const { return m_ID; }

private:
    u32 m_ID = 0;
};

} // namespace Engine
