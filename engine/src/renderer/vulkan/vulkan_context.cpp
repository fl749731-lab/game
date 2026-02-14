#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/core/log.h"

namespace Engine {

#ifdef ENGINE_ENABLE_VULKAN

// ═══════════════════════════════════════════════════════════
//  Vulkan Context 桩实现
// ═══════════════════════════════════════════════════════════
//
// 当前仅含日志占位，真正的 Vulkan 初始化将在后续版本中实现
// 需要链接 vulkan-1.lib 或 libvulkan.so

static bool s_VkInitialized = false;

bool VulkanContext::Init(const VulkanContextConfig& config) {
    LOG_INFO("[Vulkan] Initializing context: %s", config.AppName);

    if (!CreateInstance(config)) {
        LOG_ERROR("[Vulkan] Failed to create instance");
        return false;
    }

    if (!SelectPhysicalDevice()) {
        LOG_ERROR("[Vulkan] Failed to select physical device");
        return false;
    }

    if (!CreateLogicalDevice()) {
        LOG_ERROR("[Vulkan] Failed to create logical device");
        return false;
    }

    s_VkInitialized = true;
    LOG_INFO("[Vulkan] Context initialized successfully");
    return true;
}

void VulkanContext::Shutdown() {
    if (!s_VkInitialized) return;
    LOG_INFO("[Vulkan] Shutting down context");
    // TODO: vkDestroyDevice, vkDestroyInstance 等
    s_VkInitialized = false;
}

bool VulkanContext::IsInitialized() { return s_VkInitialized; }

void* VulkanContext::GetInstance()            { return nullptr; /* TODO */ }
void* VulkanContext::GetPhysicalDevice()      { return nullptr; /* TODO */ }
void* VulkanContext::GetDevice()              { return nullptr; /* TODO */ }
void* VulkanContext::GetGraphicsQueue()       { return nullptr; /* TODO */ }
u32   VulkanContext::GetGraphicsQueueFamily() { return 0;       /* TODO */ }
void* VulkanContext::GetSurface()             { return nullptr; /* TODO */ }

bool VulkanContext::CreateInstance(const VulkanContextConfig& config) {
    LOG_DEBUG("[Vulkan] Creating instance (validation=%d)", config.Validation);
    // TODO: vkCreateInstance
    return true;
}

bool VulkanContext::SelectPhysicalDevice() {
    LOG_DEBUG("[Vulkan] Selecting physical device");
    // TODO: vkEnumeratePhysicalDevices + 评分选择
    return true;
}

bool VulkanContext::CreateLogicalDevice() {
    LOG_DEBUG("[Vulkan] Creating logical device");
    // TODO: vkCreateDevice
    return true;
}

// ── Pipeline 桩 ────────────────────────────────────────────

void* VulkanPipeline::Create(const VulkanPipelineConfig& config) {
    LOG_DEBUG("[Vulkan] Creating pipeline");
    // TODO: vkCreateGraphicsPipelines
    return nullptr;
}

void VulkanPipeline::Destroy(void* pipeline) {
    // TODO: vkDestroyPipeline
}

// ── Buffer 桩 ──────────────────────────────────────────────

void* VulkanBuffer::Create(VulkanBufferType type, u64 size, const void* data) {
    LOG_DEBUG("[Vulkan] Creating buffer (type=%d, size=%llu)", (int)type, size);
    // TODO: vkCreateBuffer + vkAllocateMemory
    return nullptr;
}

void VulkanBuffer::Destroy(void* buffer) {}
void VulkanBuffer::Upload(void* buffer, const void* data, u64 size, u64 offset) {}
void* VulkanBuffer::Map(void* buffer) { return nullptr; }
void VulkanBuffer::Unmap(void* buffer) {}

// ── Command 桩 ─────────────────────────────────────────────

void* VulkanCommand::AllocateCommandBuffer() { return nullptr; }
void VulkanCommand::FreeCommandBuffer(void* cmdBuffer) {}
void VulkanCommand::Begin(void* cmdBuffer) {}
void VulkanCommand::End(void* cmdBuffer) {}
void VulkanCommand::Submit(void* cmdBuffer, bool waitIdle) {}

// ── Shader 桩 ──────────────────────────────────────────────

void* VulkanShader::LoadFromSPIRV(const char* vertPath, const char* fragPath) {
    LOG_DEBUG("[Vulkan] Loading SPIR-V shaders");
    return nullptr;
}

void* VulkanShader::LoadFromGLSL(const char* vertSrc, const char* fragSrc) {
    LOG_DEBUG("[Vulkan] Compiling GLSL → SPIR-V (runtime)");
    return nullptr;
}

void VulkanShader::Destroy(void* shaderModule) {}

// ── Swapchain 桩 ───────────────────────────────────────────

bool VulkanSwapchain::Create(u32 width, u32 height) {
    LOG_DEBUG("[Vulkan] Creating swapchain (%ux%u)", width, height);
    return true;
}

void VulkanSwapchain::Destroy() {}
void VulkanSwapchain::Resize(u32 width, u32 height) {}
u32  VulkanSwapchain::AcquireNextImage() { return 0; }
void VulkanSwapchain::Present() {}
u32  VulkanSwapchain::GetImageCount() { return 0; }

#endif // ENGINE_ENABLE_VULKAN

} // namespace Engine
