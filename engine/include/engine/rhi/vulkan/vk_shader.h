#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/rhi/rhi_shader.h"
#include <vulkan/vulkan.h>
#include <string>

namespace Engine {

class VulkanShader; // 前向声明

// ── Vulkan 着色器 (Pipeline 封装) ────────────────────────────

class VKShader : public RHIShader {
public:
    VKShader(const std::string& vertexPath, const std::string& fragmentPath);
    ~VKShader() override;

    void Bind() const override;
    void Unbind() const override;
    bool IsValid() const override { return m_Pipeline != VK_NULL_HANDLE; }

    void SetInt(const std::string& name, i32 value) override;
    void SetFloat(const std::string& name, f32 value) override;
    void SetVec2(const std::string& name, f32 x, f32 y) override;
    void SetVec3(const std::string& name, f32 x, f32 y, f32 z) override;
    void SetVec4(const std::string& name, f32 x, f32 y, f32 z, f32 w) override;
    void SetMat3(const std::string& name, const f32* value) override;
    void SetMat4(const std::string& name, const f32* value) override;

    /// Vulkan 专用
    VkPipeline       GetPipeline() const { return m_Pipeline; }
    VkPipelineLayout GetLayout()   const { return m_PipelineLayout; }

    /// 重置 Push Constants 偏移 (每帧开始时调用)
    void ResetPushConstants();

private:
    void PushData(const std::string& name, const void* data, u32 size);

    Scope<VulkanShader> m_VulkanShader; // 持有实际 Pipeline 资源
    VkPipeline       m_Pipeline       = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    mutable u32 m_PushConstantOffset  = 0;
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
