#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_texture.h"
#include "engine/renderer/vulkan/vulkan_descriptor.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <string>
#include <array>
#include <memory>

namespace Engine {

// ── PBR 材质属性 (GPU UBO 对齐) ─────────────────────────────

struct VulkanMaterialProperties {
    glm::vec3 Albedo       = {0.8f, 0.8f, 0.8f};
    f32       Metallic     = 0.0f;
    f32       Roughness    = 0.5f;
    f32       AO           = 1.0f;
    glm::vec3 Emissive     = {0.0f, 0.0f, 0.0f};
    f32       EmissiveIntensity = 0.0f;
    f32       Shininess    = 32.0f;   // Blinn-Phong 兼容
    f32       _Padding[3]  = {};      // 对齐到 64 bytes
};

// ── 纹理插槽 ────────────────────────────────────────────────

enum class VulkanTextureSlot : u8 {
    Albedo            = 0,
    Normal            = 1,
    MetallicRoughness = 2,
    AO                = 3,
    Emissive          = 4,
    Count             = 5
};

// ── Vulkan 材质 ─────────────────────────────────────────────
// 封装 PBR 属性 + 5 纹理槽 + per-material Descriptor Set

class VulkanMaterial {
public:
    VulkanMaterial();
    ~VulkanMaterial();

    // 禁止拷贝
    VulkanMaterial(const VulkanMaterial&) = delete;
    VulkanMaterial& operator=(const VulkanMaterial&) = delete;

    // ── 纹理 ───────────────────────────────────────────────
    void SetTexture(VulkanTextureSlot slot, std::shared_ptr<VulkanTexture2D> tex);
    std::shared_ptr<VulkanTexture2D> GetTexture(VulkanTextureSlot slot) const;
    bool HasTexture(VulkanTextureSlot slot) const;

    // ── 属性 ───────────────────────────────────────────────
    VulkanMaterialProperties Props;

    // ── 名称 ───────────────────────────────────────────────
    std::string Name;

    // ── UBO 更新 ───────────────────────────────────────────
    /// 将 Props 上传到 GPU UBO
    void UpdateUBO();

    // ── Descriptor Set ─────────────────────────────────────
    /// 获取 Descriptor Set (包含 UBO + 纹理 bindings)
    VkDescriptorSet GetDescriptorSet() const { return m_DescriptorSet; }

    // ── 排序键 (减少 state change) ─────────────────────────
    u64 GetSortKey() const;

    // ── 静态: Descriptor Set Layout ────────────────────────
    /// 初始化材质系统的全局 Descriptor Set Layout
    static void InitLayout();
    static void ShutdownLayout();
    static VkDescriptorSetLayout GetLayout() { return s_Layout; }

private:
    void CreateDescriptorSet();

    std::array<std::shared_ptr<VulkanTexture2D>, (u8)VulkanTextureSlot::Count> m_Textures = {};

    // per-material UBO
    VkBuffer       m_UBO       = VK_NULL_HANDLE;
    VkDeviceMemory m_UBOMemory = VK_NULL_HANDLE;
    void*          m_UBOMapped = nullptr;

    // per-material Descriptor Set
    VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;

    // 全局共享
    static VkDescriptorSetLayout s_Layout;
    static std::shared_ptr<VulkanTexture2D> s_DefaultWhiteTexture;
    static std::shared_ptr<VulkanTexture2D> s_DefaultNormalTexture;
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
