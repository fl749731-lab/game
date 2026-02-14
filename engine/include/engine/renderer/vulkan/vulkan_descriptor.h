#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>

namespace Engine {

// ── Descriptor Set Layout Builder ──────────────────────────

class VulkanDescriptorSetLayoutBuilder {
public:
    VulkanDescriptorSetLayoutBuilder& AddBinding(u32 binding, VkDescriptorType type,
                                                  VkShaderStageFlags stageFlags,
                                                  u32 count = 1);
    VkDescriptorSetLayout Build();

private:
    std::vector<VkDescriptorSetLayoutBinding> m_Bindings;
};

// ── Descriptor Pool ────────────────────────────────────────

class VulkanDescriptorPool {
public:
    static bool Init(u32 maxSets = 100);
    static void Shutdown();

    static VkDescriptorSet Allocate(VkDescriptorSetLayout layout);
    static void Free(VkDescriptorSet set);
    static void ResetPool();

    static VkDescriptorPool GetPool();

private:
    static VkDescriptorPool s_Pool;
};

// ── Descriptor Writer ──────────────────────────────────────

class VulkanDescriptorWriter {
public:
    VulkanDescriptorWriter(VkDescriptorSet set);

    VulkanDescriptorWriter& WriteBuffer(u32 binding, VkDescriptorType type,
                                         VkBuffer buffer, VkDeviceSize size,
                                         VkDeviceSize offset = 0);

    VulkanDescriptorWriter& WriteImage(u32 binding,
                                        VkImageView imageView,
                                        VkSampler sampler,
                                        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    void Flush();

private:
    VkDescriptorSet m_Set;
    std::vector<VkWriteDescriptorSet>   m_Writes;
    std::vector<VkDescriptorBufferInfo> m_BufferInfos;
    std::vector<VkDescriptorImageInfo>  m_ImageInfos;
};

// ── Per-Frame UBO 管理器 ───────────────────────────────────

struct VulkanUBO {
    VkBuffer       Buffer = VK_NULL_HANDLE;
    VkDeviceMemory Memory = VK_NULL_HANDLE;
    void*          Mapped = nullptr;
    VkDeviceSize   Size   = 0;
};

class VulkanUniformManager {
public:
    /// 创建一个 UBO (HOST_VISIBLE, 持久映射)
    static VulkanUBO CreateUBO(VkDeviceSize size);
    static void DestroyUBO(VulkanUBO& ubo);

    /// 更新 UBO 数据 (直接 memcpy 到映射内存)
    static void UpdateUBO(VulkanUBO& ubo, const void* data, VkDeviceSize size);
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
