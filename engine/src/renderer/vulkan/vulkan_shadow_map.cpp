#include "engine/renderer/vulkan/vulkan_shadow_map.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/renderer/vulkan/vulkan_texture.h"
#include "engine/core/log.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

VkImage        VulkanShadowMap::s_DepthImage    = VK_NULL_HANDLE;
VkDeviceMemory VulkanShadowMap::s_DepthMemory   = VK_NULL_HANDLE;
VkImageView    VulkanShadowMap::s_DepthView     = VK_NULL_HANDLE;
VkSampler      VulkanShadowMap::s_ShadowSampler = VK_NULL_HANDLE;
VkRenderPass   VulkanShadowMap::s_RenderPass    = VK_NULL_HANDLE;
VkFramebuffer  VulkanShadowMap::s_Framebuffer   = VK_NULL_HANDLE;

glm::mat4          VulkanShadowMap::s_LightSpaceMatrix = glm::mat4(1.0f);
VulkanShadowConfig VulkanShadowMap::s_Config = {};

// ── 初始化 ──────────────────────────────────────────────────

bool VulkanShadowMap::Init(const VulkanShadowConfig& config) {
    s_Config = config;

    if (!CreateRenderPass()) return false;
    if (!CreateDepthResources()) return false;
    if (!CreateFramebuffer()) return false;

    // 深度比较采样器 (PCF 采样)
    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor  = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    // 启用深度比较
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp     = VK_COMPARE_OP_LESS_OR_EQUAL;

    samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy    = 1.0f;

    if (vkCreateSampler(VulkanContext::GetDevice(), &samplerInfo, nullptr, &s_ShadowSampler) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] 阴影采样器创建失败");
        return false;
    }

    LOG_INFO("[Vulkan] ShadowMap 初始化完成 (%ux%u)", config.Resolution, config.Resolution);
    return true;
}

void VulkanShadowMap::Shutdown() {
    DestroyResources();
    VkDevice device = VulkanContext::GetDevice();

    if (s_ShadowSampler) {
        vkDestroySampler(device, s_ShadowSampler, nullptr);
        s_ShadowSampler = VK_NULL_HANDLE;
    }
    if (s_RenderPass) {
        vkDestroyRenderPass(device, s_RenderPass, nullptr);
        s_RenderPass = VK_NULL_HANDLE;
    }
}

// ── 阴影 Pass ───────────────────────────────────────────────

void VulkanShadowMap::BeginPass(VkCommandBuffer cmd) {
    VkClearValue clearValue = {};
    clearValue.depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpInfo = {};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = s_RenderPass;
    rpInfo.framebuffer       = s_Framebuffer;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = {s_Config.Resolution, s_Config.Resolution};
    rpInfo.clearValueCount   = 1;
    rpInfo.pClearValues      = &clearValue;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {};
    viewport.width  = (f32)s_Config.Resolution;
    viewport.height = (f32)s_Config.Resolution;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {{0, 0}, {s_Config.Resolution, s_Config.Resolution}};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void VulkanShadowMap::EndPass(VkCommandBuffer cmd) {
    vkCmdEndRenderPass(cmd);
}

void VulkanShadowMap::SetLightDirection(const glm::vec3& direction) {
    glm::vec3 lightDir = glm::normalize(direction);
    glm::vec3 lightPos = -lightDir * (s_Config.FarPlane * 0.5f);

    glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    f32 sz = s_Config.OrthoSize;
    glm::mat4 lightProj = glm::ortho(-sz, sz, -sz, sz, s_Config.NearPlane, s_Config.FarPlane);

    s_LightSpaceMatrix = lightProj * lightView;
}

VkDescriptorImageInfo VulkanShadowMap::GetDepthDescriptorInfo() {
    VkDescriptorImageInfo info = {};
    info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    info.imageView   = s_DepthView;
    info.sampler     = s_ShadowSampler;
    return info;
}

// ── 内部: RenderPass ────────────────────────────────────────

bool VulkanShadowMap::CreateRenderPass() {
    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format         = VulkanContext::FindDepthFormat();
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef = {0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 0;
    subpass.pDepthStencilAttachment = &depthRef;

    // 依赖: 确保深度写入完成后才在 fragment shader 中采样
    VkSubpassDependency dep = {};
    dep.srcSubpass    = 0;
    dep.dstSubpass    = VK_SUBPASS_EXTERNAL;
    dep.srcStageMask  = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments    = &depthAttachment;
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies   = &dep;

    if (vkCreateRenderPass(VulkanContext::GetDevice(), &rpInfo, nullptr, &s_RenderPass) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] Shadow RenderPass 创建失败");
        return false;
    }
    return true;
}

bool VulkanShadowMap::CreateDepthResources() {
    VkFormat depthFormat = VulkanContext::FindDepthFormat();

    VulkanImage::CreateImage(s_Config.Resolution, s_Config.Resolution, depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        s_DepthImage, s_DepthMemory);

    s_DepthView = VulkanImage::CreateImageView(
        s_DepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

    return s_DepthView != VK_NULL_HANDLE;
}

bool VulkanShadowMap::CreateFramebuffer() {
    VkFramebufferCreateInfo fbInfo = {};
    fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass      = s_RenderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments    = &s_DepthView;
    fbInfo.width           = s_Config.Resolution;
    fbInfo.height          = s_Config.Resolution;
    fbInfo.layers          = 1;

    if (vkCreateFramebuffer(VulkanContext::GetDevice(), &fbInfo, nullptr, &s_Framebuffer) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] Shadow Framebuffer 创建失败");
        return false;
    }
    return true;
}

void VulkanShadowMap::DestroyResources() {
    VkDevice device = VulkanContext::GetDevice();

    if (s_Framebuffer) {
        vkDestroyFramebuffer(device, s_Framebuffer, nullptr);
        s_Framebuffer = VK_NULL_HANDLE;
    }
    VulkanImage::DestroyImage(s_DepthImage, s_DepthMemory, s_DepthView);
    s_DepthImage  = VK_NULL_HANDLE;
    s_DepthMemory = VK_NULL_HANDLE;
    s_DepthView   = VK_NULL_HANDLE;
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
