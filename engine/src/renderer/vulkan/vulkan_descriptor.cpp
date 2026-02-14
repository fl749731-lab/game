#include "engine/renderer/vulkan/vulkan_descriptor.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/core/log.h"

#include <cstring>

namespace Engine {

// ═══════════════════════════════════════════════════════════
//  Descriptor Set Layout Builder
// ═══════════════════════════════════════════════════════════

VulkanDescriptorSetLayoutBuilder& VulkanDescriptorSetLayoutBuilder::AddBinding(
    u32 binding, VkDescriptorType type, VkShaderStageFlags stageFlags, u32 count) {
    VkDescriptorSetLayoutBinding b = {};
    b.binding            = binding;
    b.descriptorType     = type;
    b.descriptorCount    = count;
    b.stageFlags         = stageFlags;
    b.pImmutableSamplers = nullptr;
    m_Bindings.push_back(b);
    return *this;
}

VkDescriptorSetLayout VulkanDescriptorSetLayoutBuilder::Build() {
    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = (u32)m_Bindings.size();
    info.pBindings    = m_Bindings.data();

    VkDescriptorSetLayout layout;
    if (vkCreateDescriptorSetLayout(VulkanContext::GetDevice(), &info, nullptr, &layout) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] vkCreateDescriptorSetLayout 失败");
        return VK_NULL_HANDLE;
    }
    return layout;
}

// ═══════════════════════════════════════════════════════════
//  Descriptor Pool
// ═══════════════════════════════════════════════════════════

VkDescriptorPool VulkanDescriptorPool::s_Pool = VK_NULL_HANDLE;

bool VulkanDescriptorPool::Init(u32 maxSets) {
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         maxSets * 2 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, maxSets },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxSets * 4 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         maxSets },
    };

    VkDescriptorPoolCreateInfo info = {};
    info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    info.maxSets       = maxSets;
    info.poolSizeCount = 4;
    info.pPoolSizes    = poolSizes;

    if (vkCreateDescriptorPool(VulkanContext::GetDevice(), &info, nullptr, &s_Pool) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] vkCreateDescriptorPool 失败");
        return false;
    }
    LOG_INFO("[Vulkan] DescriptorPool 已创建 (maxSets=%u)", maxSets);
    return true;
}

void VulkanDescriptorPool::Shutdown() {
    if (s_Pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(VulkanContext::GetDevice(), s_Pool, nullptr);
        s_Pool = VK_NULL_HANDLE;
    }
}

VkDescriptorSet VulkanDescriptorPool::Allocate(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo info = {};
    info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool     = s_Pool;
    info.descriptorSetCount = 1;
    info.pSetLayouts        = &layout;

    VkDescriptorSet set;
    if (vkAllocateDescriptorSets(VulkanContext::GetDevice(), &info, &set) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] vkAllocateDescriptorSets 失败");
        return VK_NULL_HANDLE;
    }
    return set;
}

void VulkanDescriptorPool::Free(VkDescriptorSet set) {
    vkFreeDescriptorSets(VulkanContext::GetDevice(), s_Pool, 1, &set);
}

void VulkanDescriptorPool::ResetPool() {
    vkResetDescriptorPool(VulkanContext::GetDevice(), s_Pool, 0);
}

VkDescriptorPool VulkanDescriptorPool::GetPool() {
    return s_Pool;
}

// ═══════════════════════════════════════════════════════════
//  Descriptor Writer
// ═══════════════════════════════════════════════════════════

VulkanDescriptorWriter::VulkanDescriptorWriter(VkDescriptorSet set)
    : m_Set(set) {}

VulkanDescriptorWriter& VulkanDescriptorWriter::WriteBuffer(
    u32 binding, VkDescriptorType type,
    VkBuffer buffer, VkDeviceSize size, VkDeviceSize offset) {

    m_BufferInfos.push_back({buffer, offset, size});

    VkWriteDescriptorSet write = {};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = m_Set;
    write.dstBinding      = binding;
    write.dstArrayElement = 0;
    write.descriptorType  = type;
    write.descriptorCount = 1;
    // pBufferInfo 会在 Flush 时修复指针
    m_Writes.push_back(write);
    return *this;
}

VulkanDescriptorWriter& VulkanDescriptorWriter::WriteImage(
    u32 binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout) {

    m_ImageInfos.push_back({sampler, imageView, layout});

    VkWriteDescriptorSet write = {};
    write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet          = m_Set;
    write.dstBinding      = binding;
    write.dstArrayElement = 0;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    m_Writes.push_back(write);
    return *this;
}

void VulkanDescriptorWriter::Flush() {
    // 修复指针 — BufferInfo 和 ImageInfo 的索引需逐写入匹配
    u32 bufIdx = 0, imgIdx = 0;
    for (auto& w : m_Writes) {
        if (w.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            w.pImageInfo = &m_ImageInfos[imgIdx++];
        } else {
            w.pBufferInfo = &m_BufferInfos[bufIdx++];
        }
    }

    vkUpdateDescriptorSets(VulkanContext::GetDevice(),
                           (u32)m_Writes.size(), m_Writes.data(), 0, nullptr);
}

// ═══════════════════════════════════════════════════════════
//  Uniform Manager
// ═══════════════════════════════════════════════════════════

VulkanUBO VulkanUniformManager::CreateUBO(VkDeviceSize size) {
    VulkanUBO ubo;
    ubo.Size = size;

    VulkanBuffer::CreateBuffer(size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        ubo.Buffer, ubo.Memory);

    vkMapMemory(VulkanContext::GetDevice(), ubo.Memory, 0, size, 0, &ubo.Mapped);
    return ubo;
}

void VulkanUniformManager::DestroyUBO(VulkanUBO& ubo) {
    if (ubo.Memory != VK_NULL_HANDLE)
        vkUnmapMemory(VulkanContext::GetDevice(), ubo.Memory);
    VulkanBuffer::DestroyBuffer(ubo.Buffer, ubo.Memory);
    ubo = {};
}

void VulkanUniformManager::UpdateUBO(VulkanUBO& ubo, const void* data, VkDeviceSize size) {
    std::memcpy(ubo.Mapped, data, (size_t)size);
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
