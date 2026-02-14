#pragma once

#include "engine/core/types.h"
#include "engine/core/log.h"

#ifdef ENGINE_ENABLE_VULKAN

#include <vulkan/vulkan.h>

#include <vector>
#include <string>

struct GLFWwindow;

namespace Engine {

// ── Vulkan Context 配置 ────────────────────────────────────

struct VulkanContextConfig {
    const char* AppName     = "Engine App";
    u32         ApiVersion  = VK_API_VERSION_1_2;
    bool        Validation  = true;
    u32         Width       = 1280;
    u32         Height      = 720;
    GLFWwindow* Window      = nullptr;  // GLFW 窗口句柄
};

// ── Vulkan Context ─────────────────────────────────────────

class VulkanContext {
public:
    static bool Init(const VulkanContextConfig& config);
    static void Shutdown();
    static bool IsInitialized();

    // 原始句柄访问
    static VkInstance       GetInstance();
    static VkPhysicalDevice GetPhysicalDevice();
    static VkDevice         GetDevice();
    static VkQueue          GetGraphicsQueue();
    static VkQueue          GetPresentQueue();
    static u32              GetGraphicsQueueFamily();
    static u32              GetPresentQueueFamily();
    static VkSurfaceKHR     GetSurface();
    static VkCommandPool    GetCommandPool();

    // 辅助
    static u32 FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties);
    static VkFormat FindDepthFormat();

private:
    static bool CreateInstance(const VulkanContextConfig& config);
    static bool SetupDebugMessenger();
    static bool CreateSurface(GLFWwindow* window);
    static bool SelectPhysicalDevice();
    static bool CreateLogicalDevice();
    static bool CreateCommandPool();
};

// ── Vulkan Swapchain ───────────────────────────────────────

struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR Capabilities;
    std::vector<VkSurfaceFormatKHR> Formats;
    std::vector<VkPresentModeKHR> PresentModes;
};

class VulkanSwapchain {
public:
    static bool Create(u32 width, u32 height);
    static void Destroy();
    static void Recreate(u32 width, u32 height);

    static VkSwapchainKHR   GetSwapchain();
    static VkFormat         GetImageFormat();
    static VkExtent2D       GetExtent();
    static u32              GetImageCount();
    static VkImageView      GetImageView(u32 index);
    static VkFramebuffer    GetFramebuffer(u32 index);
    static VkRenderPass     GetRenderPass();

    static u32  AcquireNextImage(VkSemaphore signalSemaphore);
    static void Present(u32 imageIndex, VkSemaphore waitSemaphore);

private:
    static SwapchainSupportDetails QuerySwapSupport(VkPhysicalDevice device);
    static VkSurfaceFormatKHR ChooseFormat(const std::vector<VkSurfaceFormatKHR>& formats);
    static VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes);
    static VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& cap, u32 w, u32 h);
    static bool CreateImageViews();
    static bool CreateRenderPass();
    static bool CreateFramebuffers();
    static bool CreateDepthResources();
};

// ── Vulkan Pipeline ────────────────────────────────────────

struct VulkanPipelineConfig {
    std::vector<u8> VertexShaderSPIRV;
    std::vector<u8> FragmentShaderSPIRV;
    bool DepthTest = true;
    bool Blending  = false;
    VkRenderPass RenderPass = VK_NULL_HANDLE;
};

class VulkanPipeline {
public:
    static VkPipeline Create(const VulkanPipelineConfig& config, VkPipelineLayout& outLayout);
    static void Destroy(VkPipeline pipeline, VkPipelineLayout layout);

private:
    static VkShaderModule CreateShaderModule(const std::vector<u8>& code);
};

// ── Vulkan Buffer ──────────────────────────────────────────

enum class VulkanBufferType : u8 {
    Vertex, Index, Uniform, Storage, Staging
};

class VulkanBuffer {
public:
    static void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties,
                             VkBuffer& buffer, VkDeviceMemory& memory);
    static void DestroyBuffer(VkBuffer buffer, VkDeviceMemory memory);
    static void CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
};

// ── Vulkan Command ─────────────────────────────────────────

class VulkanCommand {
public:
    static VkCommandBuffer BeginSingleTime();
    static void EndSingleTime(VkCommandBuffer cmdBuffer);
    static std::vector<VkCommandBuffer> AllocateCommandBuffers(u32 count);
    static void FreeCommandBuffers(const std::vector<VkCommandBuffer>& buffers);
};

// ── Vulkan Renderer (帧循环) ───────────────────────────────

class VulkanRenderer {
public:
    static bool Init(const VulkanContextConfig& config);
    static void Shutdown();
    static void SetClearColor(f32 r, f32 g, f32 b, f32 a = 1.0f);
    static void BeginFrame();
    static void EndFrame();
    static bool ShouldRecreateSwapchain();
    static void OnResize(u32 width, u32 height);

    static VkCommandBuffer GetCurrentCommandBuffer();
    static u32 GetCurrentFrameIndex();

private:
    static bool CreateSyncObjects();
};

#else // !ENGINE_ENABLE_VULKAN

// ── 无 Vulkan 时的空占位 ───────────────────────────────────

namespace Engine {

class VulkanContext {
public:
    static bool Init(const void*) {
        LOG_WARN("[Vulkan] Vulkan backend disabled. Rebuild with -DENGINE_ENABLE_VULKAN=ON");
        return false;
    }
    static void Shutdown() {}
    static bool IsInitialized() { return false; }
};

class VulkanRenderer {
public:
    static bool Init(const void*) { return false; }
    static void Shutdown() {}
    static void BeginFrame() {}
    static void EndFrame() {}
};

#endif // ENGINE_ENABLE_VULKAN

} // namespace Engine
