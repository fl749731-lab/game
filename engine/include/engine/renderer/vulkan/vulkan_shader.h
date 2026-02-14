#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace Engine {

// ── Vulkan Shader ──────────────────────────────────────────
// 从 SPIR-V 文件加载着色器，管理 Pipeline + PipelineLayout

struct VulkanVertexInputDesc {
    std::vector<VkVertexInputBindingDescription>   Bindings;
    std::vector<VkVertexInputAttributeDescription> Attributes;
};

struct VulkanShaderConfig {
    std::string VertexPath;     // .spv 文件路径
    std::string FragmentPath;   // .spv 文件路径

    VulkanVertexInputDesc VertexInput;    // 顶点输入布局
    VkDescriptorSetLayout SetLayout = VK_NULL_HANDLE;

    // Push Constants
    u32 PushConstantSize = 0;   // 0 = 不使用 push constants
    VkShaderStageFlags PushConstantStages = VK_SHADER_STAGE_VERTEX_BIT;

    bool DepthTest = true;
    bool Blending  = false;
    VkCullModeFlags CullMode = VK_CULL_MODE_BACK_BIT;
    VkRenderPass RenderPass = VK_NULL_HANDLE; // VK_NULL_HANDLE = 使用 Swapchain 默认
};

class VulkanShader {
public:
    VulkanShader(const VulkanShaderConfig& config);
    ~VulkanShader();

    VulkanShader(const VulkanShader&) = delete;
    VulkanShader& operator=(const VulkanShader&) = delete;

    void Bind(VkCommandBuffer cmd) const;

    void PushConstants(VkCommandBuffer cmd, const void* data, u32 size) const;

    VkPipeline       GetPipeline() const { return m_Pipeline; }
    VkPipelineLayout GetLayout()   const { return m_PipelineLayout; }
    bool IsValid() const { return m_Pipeline != VK_NULL_HANDLE; }

    /// MeshVertex 标准顶点输入描述
    static VulkanVertexInputDesc GetMeshVertexInput();

private:
    static std::vector<u8> ReadSPIRV(const std::string& path);
    VkShaderModule CreateShaderModule(const std::vector<u8>& code);

    VkPipeline          m_Pipeline            = VK_NULL_HANDLE;
    VkPipelineLayout    m_PipelineLayout      = VK_NULL_HANDLE;
    VkShaderStageFlags  m_PushConstantStages  = VK_SHADER_STAGE_VERTEX_BIT;
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
