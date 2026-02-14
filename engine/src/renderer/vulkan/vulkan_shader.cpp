#include "engine/renderer/vulkan/vulkan_shader.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/renderer/mesh.h"
#include "engine/core/log.h"

#include <fstream>

namespace Engine {

// ── 辅助：读取 SPIR-V 文件 ─────────────────────────────────

std::vector<u8> VulkanShader::ReadSPIRV(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("[Vulkan] 无法打开 SPIR-V: %s", path.c_str());
        return {};
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<u8> buffer(fileSize);
    file.seekg(0);
    file.read((char*)buffer.data(), fileSize);
    return buffer;
}

VkShaderModule VulkanShader::CreateShaderModule(const std::vector<u8>& code) {
    VkShaderModuleCreateInfo info = {};
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode    = reinterpret_cast<const u32*>(code.data());

    VkShaderModule module;
    if (vkCreateShaderModule(VulkanContext::GetDevice(), &info, nullptr, &module) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] vkCreateShaderModule 失败");
        return VK_NULL_HANDLE;
    }
    return module;
}

// ── 标准 MeshVertex 输入 ───────────────────────────────────

VulkanVertexInputDesc VulkanShader::GetMeshVertexInput() {
    VulkanVertexInputDesc desc;

    VkVertexInputBindingDescription binding = {};
    binding.binding   = 0;
    binding.stride    = sizeof(MeshVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    desc.Bindings.push_back(binding);

    // loc 0: Position (vec3)
    desc.Attributes.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, Position)});
    // loc 1: Normal (vec3)
    desc.Attributes.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, Normal)});
    // loc 2: TexCoord (vec2)
    desc.Attributes.push_back({2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(MeshVertex, TexCoord)});
    // loc 3: Tangent (vec3)
    desc.Attributes.push_back({3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, Tangent)});
    // loc 4: Bitangent (vec3)
    desc.Attributes.push_back({4, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, Bitangent)});

    return desc;
}

// ── VulkanShader 构造 ──────────────────────────────────────

VulkanShader::VulkanShader(const VulkanShaderConfig& config)
    : m_PushConstantStages(config.PushConstantStages)
{
    auto vertCode = ReadSPIRV(config.VertexPath);
    auto fragCode = ReadSPIRV(config.FragmentPath);
    if (vertCode.empty() || fragCode.empty()) return;

    VkShaderModule vertModule = CreateShaderModule(vertCode);
    VkShaderModule fragModule = CreateShaderModule(fragCode);
    if (!vertModule || !fragModule) {
        if (vertModule) vkDestroyShaderModule(VulkanContext::GetDevice(), vertModule, nullptr);
        if (fragModule) vkDestroyShaderModule(VulkanContext::GetDevice(), fragModule, nullptr);
        return;
    }

    // Shader Stages
    VkPipelineShaderStageCreateInfo vertStage = {};
    vertStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName  = "main";

    VkPipelineShaderStageCreateInfo fragStage = {};
    fragStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName  = "main";

    VkPipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

    // Vertex Input
    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = (u32)config.VertexInput.Bindings.size();
    vertexInput.pVertexBindingDescriptions      = config.VertexInput.Bindings.data();
    vertexInput.vertexAttributeDescriptionCount = (u32)config.VertexInput.Attributes.size();
    vertexInput.pVertexAttributeDescriptions    = config.VertexInput.Attributes.data();

    // Input Assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // Dynamic State
    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates    = dynamicStates;

    // Viewport/Scissor (dynamic)
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth   = 1.0f;
    rasterizer.cullMode    = config.CullMode;
    rasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth Stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable  = config.DepthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = config.DepthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;

    // Color Blending
    VkPipelineColorBlendAttachmentState colorAttachment = {};
    colorAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (config.Blending) {
        colorAttachment.blendEnable         = VK_TRUE;
        colorAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        colorAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorAttachment;

    // Pipeline Layout
    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (config.SetLayout != VK_NULL_HANDLE) {
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts    = &config.SetLayout;
    }

    VkPushConstantRange pushRange = {};
    if (config.PushConstantSize > 0) {
        pushRange.stageFlags = config.PushConstantStages;
        pushRange.offset     = 0;
        pushRange.size       = config.PushConstantSize;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges    = &pushRange;
    }

    if (vkCreatePipelineLayout(VulkanContext::GetDevice(), &layoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] vkCreatePipelineLayout 失败");
        vkDestroyShaderModule(VulkanContext::GetDevice(), vertModule, nullptr);
        vkDestroyShaderModule(VulkanContext::GetDevice(), fragModule, nullptr);
        return;
    }

    // Graphics Pipeline
    VkRenderPass rp = (config.RenderPass != VK_NULL_HANDLE)
                      ? config.RenderPass : VulkanSwapchain::GetRenderPass();

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = stages;
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = m_PipelineLayout;
    pipelineInfo.renderPass          = rp;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(VulkanContext::GetDevice(), VK_NULL_HANDLE,
                                  1, &pipelineInfo, nullptr, &m_Pipeline) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] vkCreateGraphicsPipelines 失败");
        m_Pipeline = VK_NULL_HANDLE;
    }

    // 清理 ShaderModule (pipeline 创建后不再需要)
    vkDestroyShaderModule(VulkanContext::GetDevice(), vertModule, nullptr);
    vkDestroyShaderModule(VulkanContext::GetDevice(), fragModule, nullptr);

    if (m_Pipeline) {
        LOG_INFO("[Vulkan] Shader Pipeline 已创建: %s + %s",
                 config.VertexPath.c_str(), config.FragmentPath.c_str());
    }
}

VulkanShader::~VulkanShader() {
    VkDevice device = VulkanContext::GetDevice();
    if (m_Pipeline)       vkDestroyPipeline(device, m_Pipeline, nullptr);
    if (m_PipelineLayout) vkDestroyPipelineLayout(device, m_PipelineLayout, nullptr);
}

void VulkanShader::Bind(VkCommandBuffer cmd) const {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
}

void VulkanShader::PushConstants(VkCommandBuffer cmd, const void* data, u32 size) const {
    vkCmdPushConstants(cmd, m_PipelineLayout, m_PushConstantStages, 0, size, data);
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
