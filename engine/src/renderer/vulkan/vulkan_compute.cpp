#include "engine/renderer/vulkan/vulkan_compute.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/core/log.h"

#include <fstream>
#include <vector>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

VkCommandPool VulkanCompute::s_ComputePool       = VK_NULL_HANDLE;
VkQueue       VulkanCompute::s_ComputeQueue       = VK_NULL_HANDLE;
u32           VulkanCompute::s_ComputeQueueFamily = UINT32_MAX;

// ── 初始化 ──────────────────────────────────────────────────

bool VulkanCompute::Init() {
    VkDevice device = VulkanContext::GetDevice();

    // 尝试找到专用 Compute Queue Family
    // 如果没有，则复用 Graphics Queue
    VkPhysicalDevice physDevice = VulkanContext::GetPhysicalDevice();

    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, queueFamilies.data());

    // 优先寻找专用 Compute (非 Graphics)
    u32 graphicsFamily = VulkanContext::GetGraphicsQueueFamily();
    for (u32 i = 0; i < queueFamilyCount; i++) {
        if ((queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
            !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            i != graphicsFamily) {
            s_ComputeQueueFamily = i;
            break;
        }
    }

    // 没有专用 Compute → 回退到 Graphics Queue Family
    if (s_ComputeQueueFamily == UINT32_MAX) {
        s_ComputeQueueFamily = graphicsFamily;
        s_ComputeQueue = VulkanContext::GetGraphicsQueue();
        LOG_INFO("[Vulkan] Compute 使用 Graphics Queue Family %u", graphicsFamily);
    } else {
        vkGetDeviceQueue(device, s_ComputeQueueFamily, 0, &s_ComputeQueue);
        LOG_INFO("[Vulkan] Compute 使用专用 Queue Family %u", s_ComputeQueueFamily);
    }

    // 创建 Compute Command Pool
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = s_ComputeQueueFamily;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &s_ComputePool) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] Compute Command Pool 创建失败");
        return false;
    }

    LOG_INFO("[Vulkan] Compute Pipeline 子系统初始化完成");
    return true;
}

void VulkanCompute::Shutdown() {
    VkDevice device = VulkanContext::GetDevice();

    if (s_ComputePool) {
        vkDestroyCommandPool(device, s_ComputePool, nullptr);
        s_ComputePool = VK_NULL_HANDLE;
    }
    s_ComputeQueue = VK_NULL_HANDLE;
    s_ComputeQueueFamily = UINT32_MAX;
}

// ── Pipeline 创建 ───────────────────────────────────────────

VkPipeline VulkanCompute::CreatePipeline(VkPipelineLayout layout,
                                          const std::string& spvPath) {
    VkDevice device = VulkanContext::GetDevice();

    // 读取 SPIR-V
    std::ifstream file(spvPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR("[Vulkan] Compute shader 打开失败: %s", spvPath.c_str());
        return VK_NULL_HANDLE;
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    VkShaderModuleCreateInfo moduleInfo = {};
    moduleInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = fileSize;
    moduleInfo.pCode    = reinterpret_cast<const u32*>(buffer.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] Compute shader module 创建失败: %s", spvPath.c_str());
        return VK_NULL_HANDLE;
    }

    VkPipelineShaderStageCreateInfo stageInfo = {};
    stageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = shaderModule;
    stageInfo.pName  = "main";

    VkComputePipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage  = stageInfo;
    pipelineInfo.layout = layout;

    VkPipeline pipeline;
    VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE,
                                                1, &pipelineInfo, nullptr, &pipeline);

    vkDestroyShaderModule(device, shaderModule, nullptr);

    if (result != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] Compute pipeline 创建失败: %s", spvPath.c_str());
        return VK_NULL_HANDLE;
    }

    LOG_INFO("[Vulkan] Compute pipeline 已创建: %s", spvPath.c_str());
    return pipeline;
}

// ── Dispatch ────────────────────────────────────────────────

void VulkanCompute::Dispatch(VkCommandBuffer cmd, u32 groupX, u32 groupY, u32 groupZ) {
    vkCmdDispatch(cmd, groupX, groupY, groupZ);
}

// ── 独立 Compute 命令 ──────────────────────────────────────

VkCommandBuffer VulkanCompute::BeginCompute() {
    VkDevice device = VulkanContext::GetDevice();

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = s_ComputePool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    return cmd;
}

void VulkanCompute::EndCompute(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;

    vkQueueSubmit(s_ComputeQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(s_ComputeQueue);

    vkFreeCommandBuffers(VulkanContext::GetDevice(), s_ComputePool, 1, &cmd);
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
