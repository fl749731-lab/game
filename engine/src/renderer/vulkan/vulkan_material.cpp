#include "engine/renderer/vulkan/vulkan_material.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/core/log.h"

#include <cstring>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

VkDescriptorSetLayout VulkanMaterial::s_Layout = VK_NULL_HANDLE;
std::shared_ptr<VulkanTexture2D> VulkanMaterial::s_DefaultWhiteTexture = nullptr;
std::shared_ptr<VulkanTexture2D> VulkanMaterial::s_DefaultNormalTexture = nullptr;

// ── 静态: Layout 初始化 ────────────────────────────────────

void VulkanMaterial::InitLayout() {
    VulkanDescriptorSetLayoutBuilder builder;

    // binding 0 = 材质 UBO
    builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                       VK_SHADER_STAGE_FRAGMENT_BIT);

    // binding 1-5 = 纹理 (Albedo, Normal, MetallicRoughness, AO, Emissive)
    for (u32 i = 0; i < (u32)VulkanTextureSlot::Count; i++) {
        builder.AddBinding(1 + i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                           VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    s_Layout = builder.Build();
    if (s_Layout == VK_NULL_HANDLE) {
        LOG_ERROR("[VulkanMaterial] Layout 创建失败");
        return;
    }

    // 创建默认纹理
    // 1x1 白色
    u32 white = 0xFFFFFFFF;
    s_DefaultWhiteTexture = std::make_shared<VulkanTexture2D>(1, 1, &white, 4);

    // 1x1 默认法线 (128, 128, 255, 255 = 指向正面的法线)
    u32 defaultNormal = 0xFFFF8080;
    s_DefaultNormalTexture = std::make_shared<VulkanTexture2D>(1, 1, &defaultNormal, 4);

    LOG_INFO("[VulkanMaterial] 材质系统初始化完成");
}

void VulkanMaterial::ShutdownLayout() {
    s_DefaultWhiteTexture.reset();
    s_DefaultNormalTexture.reset();

    if (s_Layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(VulkanContext::GetDevice(), s_Layout, nullptr);
        s_Layout = VK_NULL_HANDLE;
    }
}

// ── 构造/析构 ───────────────────────────────────────────────

VulkanMaterial::VulkanMaterial() {
    // 创建 UBO
    VkDeviceSize uboSize = sizeof(VulkanMaterialProperties);
    VulkanBuffer::CreateBuffer(uboSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_UBO, m_UBOMemory);

    // 持久映射
    vkMapMemory(VulkanContext::GetDevice(), m_UBOMemory, 0, uboSize, 0, &m_UBOMapped);

    // 初始化 UBO 数据
    UpdateUBO();

    // 创建 Descriptor Set
    CreateDescriptorSet();
}

VulkanMaterial::~VulkanMaterial() {
    VkDevice device = VulkanContext::GetDevice();
    if (m_UBOMapped) {
        vkUnmapMemory(device, m_UBOMemory);
        m_UBOMapped = nullptr;
    }
    VulkanBuffer::DestroyBuffer(m_UBO, m_UBOMemory);
    // DescriptorSet 由 Pool 统一管理，不需要单独释放
}

// ── 纹理 ────────────────────────────────────────────────────

void VulkanMaterial::SetTexture(VulkanTextureSlot slot, std::shared_ptr<VulkanTexture2D> tex) {
    m_Textures[(u8)slot] = tex;
    // 需要重新构建 Descriptor Set
    CreateDescriptorSet();
}

std::shared_ptr<VulkanTexture2D> VulkanMaterial::GetTexture(VulkanTextureSlot slot) const {
    return m_Textures[(u8)slot];
}

bool VulkanMaterial::HasTexture(VulkanTextureSlot slot) const {
    return m_Textures[(u8)slot] != nullptr;
}

// ── UBO 更新 ────────────────────────────────────────────────

void VulkanMaterial::UpdateUBO() {
    if (m_UBOMapped) {
        std::memcpy(m_UBOMapped, &Props, sizeof(VulkanMaterialProperties));
    }
}

// ── 排序键 ──────────────────────────────────────────────────

u64 VulkanMaterial::GetSortKey() const {
    // 简单排序: 按有纹理的位图 + 属性哈希
    u64 texBits = 0;
    for (u32 i = 0; i < (u32)VulkanTextureSlot::Count; i++) {
        if (m_Textures[i]) texBits |= (1ull << i);
    }
    return texBits;
}

// ── Descriptor Set 创建 ─────────────────────────────────────

void VulkanMaterial::CreateDescriptorSet() {
    if (s_Layout == VK_NULL_HANDLE) return;

    // 从全局 Descriptor Pool 分配
    m_DescriptorSet = VulkanDescriptorPool::Allocate(s_Layout);
    if (m_DescriptorSet == VK_NULL_HANDLE) {
        LOG_ERROR("[VulkanMaterial] Descriptor Set 分配失败");
        return;
    }

    // 准备写入
    VkDescriptorBufferInfo uboInfo = {};
    uboInfo.buffer = m_UBO;
    uboInfo.offset = 0;
    uboInfo.range  = sizeof(VulkanMaterialProperties);

    // 获取纹理 descriptor info，未设置的使用默认纹理
    std::array<VkDescriptorImageInfo, (u8)VulkanTextureSlot::Count> texInfos;
    for (u32 i = 0; i < (u32)VulkanTextureSlot::Count; i++) {
        if (m_Textures[i] && m_Textures[i]->IsValid()) {
            texInfos[i] = m_Textures[i]->GetDescriptorInfo();
        } else if (i == (u32)VulkanTextureSlot::Normal && s_DefaultNormalTexture) {
            texInfos[i] = s_DefaultNormalTexture->GetDescriptorInfo();
        } else if (s_DefaultWhiteTexture) {
            texInfos[i] = s_DefaultWhiteTexture->GetDescriptorInfo();
        }
    }

    // 写入 descriptor set
    std::array<VkWriteDescriptorSet, 1 + (u8)VulkanTextureSlot::Count> writes = {};

    // UBO (binding 0)
    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = m_DescriptorSet;
    writes[0].dstBinding      = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo     = &uboInfo;

    // 纹理 (binding 1-5)
    for (u32 i = 0; i < (u32)VulkanTextureSlot::Count; i++) {
        writes[1 + i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1 + i].dstSet          = m_DescriptorSet;
        writes[1 + i].dstBinding      = 1 + i;
        writes[1 + i].dstArrayElement = 0;
        writes[1 + i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1 + i].descriptorCount = 1;
        writes[1 + i].pImageInfo      = &texInfos[i];
    }

    vkUpdateDescriptorSets(VulkanContext::GetDevice(),
                           (u32)writes.size(), writes.data(), 0, nullptr);
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
