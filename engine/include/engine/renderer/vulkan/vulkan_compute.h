#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <functional>

namespace Engine {

// ── Vulkan Compute Pipeline ────────────────────────────────
// 通用计算管线基础设施
// 用于 IBL 预计算、粒子模拟、后处理等 GPU 计算任务

struct VulkanComputeConfig {
    std::string ShaderPath;
    u32 PushConstantSize = 0;
    VkShaderStageFlags PushConstantStages = VK_SHADER_STAGE_COMPUTE_BIT;
};

class VulkanCompute {
public:
    /// 初始化计算子系统 (创建 Compute Command Pool)
    static bool Init();
    static void Shutdown();

    /// 创建计算管线
    static VkPipeline CreatePipeline(VkPipelineLayout layout,
                                     const std::string& spvPath);

    /// 提交计算作业 (异步)
    static void Dispatch(VkCommandBuffer cmd, u32 groupX, u32 groupY, u32 groupZ);

    /// 在独立的 Compute Queue 上提交 (如果设备支持)
    static VkCommandBuffer BeginCompute();
    static void EndCompute(VkCommandBuffer cmd);

    /// 获取 Compute Command Pool
    static VkCommandPool GetCommandPool() { return s_ComputePool; }

    /// 是否有独立的 Compute Queue
    static bool HasDedicatedComputeQueue() { return s_ComputeQueue != VK_NULL_HANDLE; }
    static VkQueue GetComputeQueue() { return s_ComputeQueue; }
    static u32 GetComputeQueueFamily() { return s_ComputeQueueFamily; }

private:
    static VkCommandPool s_ComputePool;
    static VkQueue       s_ComputeQueue;
    static u32           s_ComputeQueueFamily;
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
