#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/rhi/rhi_buffer.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace Engine {

// ── Vulkan 顶点缓冲 ────────────────────────────────────────
// 封装 VulkanBuffer，使用 Staging Buffer 上传到 Device Local

class VKVertexBuffer : public RHIVertexBuffer {
public:
    VKVertexBuffer(const void* data, u32 size, RHIBufferUsage usage = RHIBufferUsage::Static);
    ~VKVertexBuffer() override;

    void Bind() const override;
    void Unbind() const override;
    void SetData(const void* data, u32 size) override;
    u32  GetSize() const override { return m_Size; }

    /// Vulkan 专用: 绑定到 CommandBuffer
    VkBuffer GetVkBuffer() const { return m_Buffer; }
    void BindToCommandBuffer(VkCommandBuffer cmd) const;

private:
    VkBuffer       m_Buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;
    u32 m_Size = 0;
};

// ── Vulkan 索引缓冲 ────────────────────────────────────────

class VKIndexBuffer : public RHIIndexBuffer {
public:
    VKIndexBuffer(const u32* indices, u32 count);
    ~VKIndexBuffer() override;

    void Bind() const override;
    void Unbind() const override;
    u32  GetCount() const override { return m_Count; }

    /// Vulkan 专用: 绑定到 CommandBuffer
    VkBuffer GetVkBuffer() const { return m_Buffer; }
    void BindToCommandBuffer(VkCommandBuffer cmd) const;

private:
    VkBuffer       m_Buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_Memory = VK_NULL_HANDLE;
    u32 m_Count = 0;
};

// ── Vulkan 顶点数组 (概念封装) ─────────────────────────────
// Vulkan 没有 VAO，此类仅记录属性布局供 Pipeline 创建使用

struct VKVertexAttribute {
    u32 Index;
    i32 Size;
    i32 Stride;
    u64 Offset;
};

class VKVertexArray : public RHIVertexArray {
public:
    VKVertexArray() = default;
    ~VKVertexArray() override = default;

    void Bind() const override;
    void Unbind() const override;
    void AddAttribute(u32 index, i32 size, i32 stride, u64 offset) override;

    const std::vector<VKVertexAttribute>& GetAttributes() const { return m_Attributes; }

private:
    std::vector<VKVertexAttribute> m_Attributes;
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
