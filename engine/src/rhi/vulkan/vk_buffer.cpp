#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/rhi/vulkan/vk_buffer.h"
#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/core/log.h"

#include <cstring>

namespace Engine {

// ── VKVertexBuffer ──────────────────────────────────────────

VKVertexBuffer::VKVertexBuffer(const void* data, u32 size, RHIBufferUsage usage)
    : m_Size(size)
{
    // 创建 Staging Buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VulkanBuffer::CreateBuffer(size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory);

    // 复制数据到 Staging
    void* mapped;
    vkMapMemory(VulkanContext::GetDevice(), stagingMemory, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(VulkanContext::GetDevice(), stagingMemory);

    // 创建 Device Local Buffer
    VulkanBuffer::CreateBuffer(size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_Buffer, m_Memory);

    VulkanBuffer::CopyBuffer(stagingBuffer, m_Buffer, size);
    VulkanBuffer::DestroyBuffer(stagingBuffer, stagingMemory);
}

VKVertexBuffer::~VKVertexBuffer() {
    if (m_Buffer != VK_NULL_HANDLE) {
        VulkanBuffer::DestroyBuffer(m_Buffer, m_Memory);
    }
}

void VKVertexBuffer::Bind() const {
    // Vulkan 中通过 CommandBuffer 绑定, 此处为兼容接口
}

void VKVertexBuffer::Unbind() const {}

void VKVertexBuffer::BindToCommandBuffer(VkCommandBuffer cmd) const {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_Buffer, &offset);
}

void VKVertexBuffer::SetData(const void* data, u32 size) {
    if (!data || size == 0) {
        LOG_WARN("[VKVertexBuffer] SetData: 无效参数");
        return;
    }

    VkDevice device = VulkanContext::GetDevice();

    // 如果新 size 超过当前 buffer, 重建
    if (size > m_Size) {
        vkDeviceWaitIdle(device);
        VulkanBuffer::DestroyBuffer(m_Buffer, m_Memory);

        VulkanBuffer::CreateBuffer(size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_Buffer, m_Memory);
        m_Size = size;
    }

    // 通过 Staging Buffer 上传
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VulkanBuffer::CreateBuffer(size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory);

    void* mapped;
    vkMapMemory(device, stagingMemory, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(device, stagingMemory);

    VulkanBuffer::CopyBuffer(stagingBuffer, m_Buffer, size);
    VulkanBuffer::DestroyBuffer(stagingBuffer, stagingMemory);
}

// ── VKIndexBuffer ───────────────────────────────────────────

VKIndexBuffer::VKIndexBuffer(const u32* indices, u32 count)
    : m_Count(count)
{
    VkDeviceSize size = count * sizeof(u32);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VulkanBuffer::CreateBuffer(size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory);

    void* mapped;
    vkMapMemory(VulkanContext::GetDevice(), stagingMemory, 0, size, 0, &mapped);
    memcpy(mapped, indices, size);
    vkUnmapMemory(VulkanContext::GetDevice(), stagingMemory);

    VulkanBuffer::CreateBuffer(size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_Buffer, m_Memory);

    VulkanBuffer::CopyBuffer(stagingBuffer, m_Buffer, size);
    VulkanBuffer::DestroyBuffer(stagingBuffer, stagingMemory);
}

VKIndexBuffer::~VKIndexBuffer() {
    if (m_Buffer != VK_NULL_HANDLE) {
        VulkanBuffer::DestroyBuffer(m_Buffer, m_Memory);
    }
}

void VKIndexBuffer::Bind() const {}
void VKIndexBuffer::Unbind() const {}

void VKIndexBuffer::BindToCommandBuffer(VkCommandBuffer cmd) const {
    vkCmdBindIndexBuffer(cmd, m_Buffer, 0, VK_INDEX_TYPE_UINT32);
}

// ── VKVertexArray ───────────────────────────────────────────

void VKVertexArray::Bind() const {}
void VKVertexArray::Unbind() const {}
void VKVertexArray::AddAttribute(u32 index, i32 size, i32 stride, u64 offset) {
    // Vulkan 顶点输入通过 Pipeline 配置, 此处仅记录
    m_Attributes.push_back({index, size, stride, offset});
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
