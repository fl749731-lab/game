#pragma once

#include "engine/rhi/rhi_types.h"

namespace Engine {

// ── 抽象顶点缓冲 ────────────────────────────────────────────

class RHIVertexBuffer {
public:
    virtual ~RHIVertexBuffer() = default;

    virtual void Bind() const = 0;
    virtual void Unbind() const = 0;

    /// 更新缓冲数据 (Dynamic/Stream 用途)
    virtual void SetData(const void* data, u32 size) = 0;

    /// 获取缓冲大小 (字节)
    virtual u32 GetSize() const = 0;
};

// ── 抽象索引缓冲 ────────────────────────────────────────────

class RHIIndexBuffer {
public:
    virtual ~RHIIndexBuffer() = default;

    virtual void Bind() const = 0;
    virtual void Unbind() const = 0;

    virtual u32 GetCount() const = 0;
};

// ── 抽象顶点数组 ────────────────────────────────────────────
// 注: Vulkan 没有 VAO 概念，此抽象封装顶点输入绑定状态

class RHIVertexArray {
public:
    virtual ~RHIVertexArray() = default;

    virtual void Bind() const = 0;
    virtual void Unbind() const = 0;

    /// 添加顶点属性
    /// @param index   属性索引
    /// @param size    分量数 (1, 2, 3, 4)
    /// @param stride  步长 (字节)
    /// @param offset  偏移 (字节)
    virtual void AddAttribute(u32 index, i32 size, i32 stride, u64 offset) = 0;
};

} // namespace Engine
