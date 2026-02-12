#pragma once

#include "engine/core/types.h"
#include <vector>

namespace Engine {

// ── 顶点缓冲 (VBO) ─────────────────────────────────────────

class VertexBuffer {
public:
    VertexBuffer(const float* vertices, u32 size);
    ~VertexBuffer();

    void Bind() const;
    void Unbind() const;

    u32 GetID() const { return m_ID; }

private:
    u32 m_ID = 0;
};

// ── 索引缓冲 (IBO/EBO) ─────────────────────────────────────

class IndexBuffer {
public:
    IndexBuffer(const u32* indices, u32 count);
    ~IndexBuffer();

    void Bind() const;
    void Unbind() const;

    u32 GetCount() const { return m_Count; }
    u32 GetID() const { return m_ID; }

private:
    u32 m_ID = 0;
    u32 m_Count = 0;
};

// ── 顶点数组 (VAO) ─────────────────────────────────────────

class VertexArray {
public:
    VertexArray();
    ~VertexArray();

    void Bind() const;
    void Unbind() const;

    /// 添加顶点属性
    /// @param index   属性索引
    /// @param size    分量数 (1, 2, 3, 4)
    /// @param stride  步长 (字节)
    /// @param offset  偏移 (字节)
    void AddAttribute(u32 index, i32 size, i32 stride, u64 offset);

    u32 GetID() const { return m_ID; }

private:
    u32 m_ID = 0;
};

} // namespace Engine
