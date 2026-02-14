#include "engine/renderer/vulkan/vulkan_context.h"

#ifdef ENGINE_ENABLE_VULKAN

#include <GLFW/glfw3.h>
#include <cstring>
#include <set>
#include <algorithm>

namespace Engine {

// ═══════════════════════════════════════════════════════════
//  静态成员
// ═══════════════════════════════════════════════════════════

static VkInstance               s_Instance       = VK_NULL_HANDLE;
static VkDebugUtilsMessengerEXT s_DebugMessenger = VK_NULL_HANDLE;
static VkSurfaceKHR             s_Surface        = VK_NULL_HANDLE;
static VkPhysicalDevice         s_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice                 s_Device         = VK_NULL_HANDLE;
static VkQueue                  s_GraphicsQueue  = VK_NULL_HANDLE;
static VkQueue                  s_PresentQueue   = VK_NULL_HANDLE;
static u32                      s_GraphicsFamily = 0;
static u32                      s_PresentFamily  = 0;
static VkCommandPool            s_CommandPool    = VK_NULL_HANDLE;
static bool                     s_Initialized    = false;
static VkPhysicalDeviceProperties s_DeviceProps  = {};

// Swapchain 静态
static VkSwapchainKHR            s_Swapchain     = VK_NULL_HANDLE;
static VkFormat                  s_SwapFormat     = VK_FORMAT_B8G8R8A8_SRGB;
static VkExtent2D                s_SwapExtent     = {0, 0};
static std::vector<VkImage>      s_SwapImages;
static std::vector<VkImageView>  s_SwapImageViews;
static std::vector<VkFramebuffer> s_SwapFramebuffers;
static VkRenderPass              s_RenderPass     = VK_NULL_HANDLE;
static VkImage                   s_DepthImage     = VK_NULL_HANDLE;
static VkDeviceMemory            s_DepthMemory    = VK_NULL_HANDLE;
static VkImageView               s_DepthView      = VK_NULL_HANDLE;

// Renderer 静态
static constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;
static std::vector<VkCommandBuffer> s_CommandBuffers;
static std::vector<VkSemaphore>     s_ImageAvailable;
static std::vector<VkSemaphore>     s_RenderFinished;
static std::vector<VkFence>         s_InFlightFences;
static u32                          s_CurrentFrame  = 0;
static u32                          s_CurrentImage  = 0;
static bool                         s_NeedResize    = false;

// 验证层
static const std::vector<const char*> s_ValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
static const std::vector<const char*> s_DeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// ── Debug Callback ─────────────────────────────────────────

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* userData)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOG_WARN("[Vulkan Validation] %s", data->pMessage);
    }
    return VK_FALSE;
}

// ── VulkanContext ──────────────────────────────────────────

bool VulkanContext::Init(const VulkanContextConfig& config) {
    if (s_Initialized) return true;

    if (!CreateInstance(config)) return false;
    if (config.Validation) SetupDebugMessenger();
    if (config.Window && !CreateSurface(config.Window)) return false;
    if (!SelectPhysicalDevice()) return false;
    if (!CreateLogicalDevice()) return false;
    if (!CreateCommandPool()) return false;

    s_Initialized = true;
    LOG_INFO("[Vulkan] Context 初始化完成 — GPU: %s", s_DeviceProps.deviceName);
    return true;
}

void VulkanContext::Shutdown() {
    if (!s_Initialized) return;
    vkDeviceWaitIdle(s_Device);

    if (s_CommandPool) vkDestroyCommandPool(s_Device, s_CommandPool, nullptr);

    vkDestroyDevice(s_Device, nullptr);

    if (s_DebugMessenger) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(s_Instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(s_Instance, s_DebugMessenger, nullptr);
    }

    if (s_Surface) vkDestroySurfaceKHR(s_Instance, s_Surface, nullptr);
    vkDestroyInstance(s_Instance, nullptr);

    s_Initialized = false;
    LOG_INFO("[Vulkan] Context 已清理");
}

bool VulkanContext::IsInitialized() { return s_Initialized; }

VkInstance       VulkanContext::GetInstance()            { return s_Instance; }
VkPhysicalDevice VulkanContext::GetPhysicalDevice()     { return s_PhysicalDevice; }
VkDevice         VulkanContext::GetDevice()             { return s_Device; }
VkQueue          VulkanContext::GetGraphicsQueue()      { return s_GraphicsQueue; }
VkQueue          VulkanContext::GetPresentQueue()       { return s_PresentQueue; }
u32              VulkanContext::GetGraphicsQueueFamily() { return s_GraphicsFamily; }
u32              VulkanContext::GetPresentQueueFamily()  { return s_PresentFamily; }
VkSurfaceKHR     VulkanContext::GetSurface()            { return s_Surface; }
VkCommandPool    VulkanContext::GetCommandPool()        { return s_CommandPool; }

// ── Instance ───────────────────────────────────────────────

bool VulkanContext::CreateInstance(const VulkanContextConfig& config) {
    VkApplicationInfo appInfo = {};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = config.AppName;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = config.ApiVersion;

    // GLFW 扩展
    u32 glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> extensions(glfwExts, glfwExts + glfwExtCount);

    if (config.Validation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = (u32)extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (config.Validation) {
        createInfo.enabledLayerCount   = (u32)s_ValidationLayers.size();
        createInfo.ppEnabledLayerNames = s_ValidationLayers.data();
    }

    VkResult result = vkCreateInstance(&createInfo, nullptr, &s_Instance);
    if (result != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] vkCreateInstance 失败: %d", result);
        return false;
    }
    return true;
}

bool VulkanContext::SetupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                         | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                     | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                     | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = DebugCallback;

    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(s_Instance, "vkCreateDebugUtilsMessengerEXT");
    if (func && func(s_Instance, &info, nullptr, &s_DebugMessenger) == VK_SUCCESS) {
        LOG_DEBUG("[Vulkan] 验证层 Debug Messenger 已创建");
        return true;
    }
    return false;
}

bool VulkanContext::CreateSurface(GLFWwindow* window) {
    VkResult result = glfwCreateWindowSurface(s_Instance, window, nullptr, &s_Surface);
    if (result != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] 创建 Surface 失败: %d", result);
        return false;
    }
    return true;
}

// ── Physical Device ────────────────────────────────────────

bool VulkanContext::SelectPhysicalDevice() {
    u32 deviceCount = 0;
    vkEnumeratePhysicalDevices(s_Instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOG_ERROR("[Vulkan] 没有找到支持 Vulkan 的 GPU");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(s_Instance, &deviceCount, devices.data());

    // 简单评分：独显优先
    i32 bestScore = -1;
    for (auto& dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);

        i32 score = 0;
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
        score += (i32)(props.limits.maxImageDimension2D / 1000);

        // 检查队列族
        u32 queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());

        bool hasGraphics = false;
        for (u32 i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) hasGraphics = true;
        }
        if (!hasGraphics) continue;

        // 检查设备扩展
        u32 extCount = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, exts.data());

        std::set<std::string> required(s_DeviceExtensions.begin(), s_DeviceExtensions.end());
        for (auto& e : exts) required.erase(e.extensionName);
        if (!required.empty()) continue;

        if (score > bestScore) {
            bestScore = score;
            s_PhysicalDevice = dev;
            s_DeviceProps = props;
        }
    }

    if (s_PhysicalDevice == VK_NULL_HANDLE) {
        LOG_ERROR("[Vulkan] 没有合适的 GPU");
        return false;
    }

    // 找到图形和展示队列族
    u32 qfCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(s_PhysicalDevice, &qfCount, nullptr);
    std::vector<VkQueueFamilyProperties> qfProps(qfCount);
    vkGetPhysicalDeviceQueueFamilyProperties(s_PhysicalDevice, &qfCount, qfProps.data());

    for (u32 i = 0; i < qfCount; i++) {
        if (qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            s_GraphicsFamily = i;
        }
        if (s_Surface) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(s_PhysicalDevice, i, s_Surface, &presentSupport);
            if (presentSupport) s_PresentFamily = i;
        }
    }

    LOG_INFO("[Vulkan] 选择 GPU: %s", s_DeviceProps.deviceName);
    return true;
}

// ── Logical Device ─────────────────────────────────────────

bool VulkanContext::CreateLogicalDevice() {
    std::set<u32> uniqueFamilies = {s_GraphicsFamily, s_PresentFamily};
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    f32 queuePriority = 1.0f;

    for (u32 family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount       = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueInfos.push_back(queueInfo);
    }

    VkPhysicalDeviceFeatures features = {};
    features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = (u32)queueInfos.size();
    createInfo.pQueueCreateInfos       = queueInfos.data();
    createInfo.pEnabledFeatures        = &features;
    createInfo.enabledExtensionCount   = (u32)s_DeviceExtensions.size();
    createInfo.ppEnabledExtensionNames = s_DeviceExtensions.data();

    VkResult result = vkCreateDevice(s_PhysicalDevice, &createInfo, nullptr, &s_Device);
    if (result != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] vkCreateDevice 失败: %d", result);
        return false;
    }

    vkGetDeviceQueue(s_Device, s_GraphicsFamily, 0, &s_GraphicsQueue);
    vkGetDeviceQueue(s_Device, s_PresentFamily, 0, &s_PresentQueue);
    return true;
}

bool VulkanContext::CreateCommandPool() {
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = s_GraphicsFamily;

    VkResult result = vkCreateCommandPool(s_Device, &poolInfo, nullptr, &s_CommandPool);
    if (result != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] 创建 CommandPool 失败");
        return false;
    }
    return true;
}

u32 VulkanContext::FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(s_PhysicalDevice, &memProps);
    for (u32 i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    LOG_ERROR("[Vulkan] 找不到合适的内存类型");
    return 0;
}

VkFormat VulkanContext::FindDepthFormat() {
    VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    for (auto fmt : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(s_PhysicalDevice, fmt, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return fmt;
        }
    }
    return VK_FORMAT_D32_SFLOAT;
}

// ═══════════════════════════════════════════════════════════
//  Swapchain
// ═══════════════════════════════════════════════════════════

SwapchainSupportDetails VulkanSwapchain::QuerySwapSupport(VkPhysicalDevice device) {
    SwapchainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, s_Surface, &details.Capabilities);

    u32 formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, s_Surface, &formatCount, nullptr);
    if (formatCount) {
        details.Formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, s_Surface, &formatCount, details.Formats.data());
    }

    u32 modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, s_Surface, &modeCount, nullptr);
    if (modeCount) {
        details.PresentModes.resize(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, s_Surface, &modeCount, details.PresentModes.data());
    }
    return details;
}

VkSurfaceFormatKHR VulkanSwapchain::ChooseFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    }
    return formats[0];
}

VkPresentModeKHR VulkanSwapchain::ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
    for (auto m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::ChooseExtent(const VkSurfaceCapabilitiesKHR& cap, u32 w, u32 h) {
    if (cap.currentExtent.width != UINT32_MAX) return cap.currentExtent;
    VkExtent2D ext = {w, h};
    ext.width  = std::max(cap.minImageExtent.width, std::min(cap.maxImageExtent.width, ext.width));
    ext.height = std::max(cap.minImageExtent.height, std::min(cap.maxImageExtent.height, ext.height));
    return ext;
}

bool VulkanSwapchain::Create(u32 width, u32 height) {
    auto support = QuerySwapSupport(s_PhysicalDevice);
    auto format  = ChooseFormat(support.Formats);
    auto mode    = ChoosePresentMode(support.PresentModes);
    auto extent  = ChooseExtent(support.Capabilities, width, height);

    u32 imageCount = support.Capabilities.minImageCount + 1;
    if (support.Capabilities.maxImageCount > 0 && imageCount > support.Capabilities.maxImageCount)
        imageCount = support.Capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = s_Surface;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = format.format;
    createInfo.imageColorSpace  = format.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    u32 queueFamilyIndices[] = {s_GraphicsFamily, s_PresentFamily};
    if (s_GraphicsFamily != s_PresentFamily) {
        createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices   = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform   = support.Capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode    = mode;
    createInfo.clipped        = VK_TRUE;
    createInfo.oldSwapchain   = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(s_Device, &createInfo, nullptr, &s_Swapchain);
    if (result != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] 创建 Swapchain 失败: %d", result);
        return false;
    }

    vkGetSwapchainImagesKHR(s_Device, s_Swapchain, &imageCount, nullptr);
    s_SwapImages.resize(imageCount);
    vkGetSwapchainImagesKHR(s_Device, s_Swapchain, &imageCount, s_SwapImages.data());

    s_SwapFormat = format.format;
    s_SwapExtent = extent;

    if (!CreateImageViews()) return false;
    if (!CreateRenderPass()) return false;
    if (!CreateDepthResources()) return false;
    if (!CreateFramebuffers()) return false;

    LOG_INFO("[Vulkan] Swapchain 创建: %ux%u, %u images", extent.width, extent.height, imageCount);
    return true;
}

void VulkanSwapchain::Destroy() {
    vkDeviceWaitIdle(s_Device);

    if (s_DepthView)   vkDestroyImageView(s_Device, s_DepthView, nullptr);
    if (s_DepthImage)  vkDestroyImage(s_Device, s_DepthImage, nullptr);
    if (s_DepthMemory) vkFreeMemory(s_Device, s_DepthMemory, nullptr);

    for (auto fb : s_SwapFramebuffers) vkDestroyFramebuffer(s_Device, fb, nullptr);
    if (s_RenderPass) vkDestroyRenderPass(s_Device, s_RenderPass, nullptr);
    for (auto iv : s_SwapImageViews)   vkDestroyImageView(s_Device, iv, nullptr);
    if (s_Swapchain)  vkDestroySwapchainKHR(s_Device, s_Swapchain, nullptr);

    s_SwapFramebuffers.clear();
    s_SwapImageViews.clear();
    s_SwapImages.clear();
    s_RenderPass = VK_NULL_HANDLE;
    s_Swapchain  = VK_NULL_HANDLE;
}

void VulkanSwapchain::Recreate(u32 width, u32 height) {
    vkDeviceWaitIdle(s_Device);
    Destroy();
    Create(width, height);
}

VkSwapchainKHR VulkanSwapchain::GetSwapchain()   { return s_Swapchain; }
VkFormat       VulkanSwapchain::GetImageFormat()  { return s_SwapFormat; }
VkExtent2D     VulkanSwapchain::GetExtent()       { return s_SwapExtent; }
u32            VulkanSwapchain::GetImageCount()   { return (u32)s_SwapImages.size(); }
VkImageView    VulkanSwapchain::GetImageView(u32 i)   { return s_SwapImageViews[i]; }
VkFramebuffer  VulkanSwapchain::GetFramebuffer(u32 i) { return s_SwapFramebuffers[i]; }
VkRenderPass   VulkanSwapchain::GetRenderPass()       { return s_RenderPass; }

bool VulkanSwapchain::CreateImageViews() {
    s_SwapImageViews.resize(s_SwapImages.size());
    for (size_t i = 0; i < s_SwapImages.size(); i++) {
        VkImageViewCreateInfo info = {};
        info.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image    = s_SwapImages[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format   = s_SwapFormat;
        info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel   = 0;
        info.subresourceRange.levelCount     = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount     = 1;
        if (vkCreateImageView(s_Device, &info, nullptr, &s_SwapImageViews[i]) != VK_SUCCESS) {
            LOG_ERROR("[Vulkan] 创建 ImageView 失败");
            return false;
        }
    }
    return true;
}

bool VulkanSwapchain::CreateRenderPass() {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format         = s_SwapFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment = {};
    depthAttachment.format         = VulkanContext::FindDepthFormat();
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 2;
    rpInfo.pAttachments    = attachments;
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies   = &dependency;

    if (vkCreateRenderPass(s_Device, &rpInfo, nullptr, &s_RenderPass) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] 创建 RenderPass 失败");
        return false;
    }
    return true;
}

bool VulkanSwapchain::CreateDepthResources() {
    VkFormat depthFormat = VulkanContext::FindDepthFormat();

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = s_SwapExtent.width;
    imageInfo.extent.height = s_SwapExtent.height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = depthFormat;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;

    vkCreateImage(s_Device, &imageInfo, nullptr, &s_DepthImage);

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(s_Device, s_DepthImage, &memReq);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = VulkanContext::FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(s_Device, &allocInfo, nullptr, &s_DepthMemory);
    vkBindImageMemory(s_Device, s_DepthImage, s_DepthMemory, 0);

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image    = s_DepthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format   = depthFormat;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;
    vkCreateImageView(s_Device, &viewInfo, nullptr, &s_DepthView);

    return true;
}

bool VulkanSwapchain::CreateFramebuffers() {
    s_SwapFramebuffers.resize(s_SwapImageViews.size());
    for (size_t i = 0; i < s_SwapImageViews.size(); i++) {
        VkImageView attachments[] = {s_SwapImageViews[i], s_DepthView};

        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = s_RenderPass;
        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments    = attachments;
        fbInfo.width           = s_SwapExtent.width;
        fbInfo.height          = s_SwapExtent.height;
        fbInfo.layers          = 1;

        if (vkCreateFramebuffer(s_Device, &fbInfo, nullptr, &s_SwapFramebuffers[i]) != VK_SUCCESS) {
            LOG_ERROR("[Vulkan] 创建 Framebuffer 失败");
            return false;
        }
    }
    return true;
}

u32 VulkanSwapchain::AcquireNextImage(VkSemaphore signalSemaphore) {
    u32 imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(s_Device, s_Swapchain, UINT64_MAX, signalSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        s_NeedResize = true;
        return UINT32_MAX; // 无效索引，调用方需检查
    }
    if (result == VK_SUBOPTIMAL_KHR) {
        s_NeedResize = true;
        // 仍可继续使用当前帧
    }
    return imageIndex;
}

void VulkanSwapchain::Present(u32 imageIndex, VkSemaphore waitSemaphore) {
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &waitSemaphore;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &s_Swapchain;
    presentInfo.pImageIndices      = &imageIndex;

    VkResult result = vkQueuePresentKHR(s_PresentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        s_NeedResize = true;
    }
}

// ═══════════════════════════════════════════════════════════
//  Buffer
// ═══════════════════════════════════════════════════════════

void VulkanBuffer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                 VkMemoryPropertyFlags properties,
                                 VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo bufInfo = {};
    bufInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size        = size;
    bufInfo.usage       = usage;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(s_Device, &bufInfo, nullptr, &buffer);

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(s_Device, buffer, &memReq);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = VulkanContext::FindMemoryType(memReq.memoryTypeBits, properties);
    vkAllocateMemory(s_Device, &allocInfo, nullptr, &memory);
    vkBindBufferMemory(s_Device, buffer, memory, 0);
}

void VulkanBuffer::DestroyBuffer(VkBuffer buffer, VkDeviceMemory memory) {
    vkDestroyBuffer(s_Device, buffer, nullptr);
    vkFreeMemory(s_Device, memory, nullptr);
}

void VulkanBuffer::CopyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    VkCommandBuffer cmd = VulkanCommand::BeginSingleTime();
    VkBufferCopy region = {0, 0, size};
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    VulkanCommand::EndSingleTime(cmd);
}

// ═══════════════════════════════════════════════════════════
//  Command
// ═══════════════════════════════════════════════════════════

VkCommandBuffer VulkanCommand::BeginSingleTime() {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = s_CommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(s_Device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);
    return cmd;
}

void VulkanCommand::EndSingleTime(VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submitInfo = {};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;
    vkQueueSubmit(s_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(s_GraphicsQueue);
    vkFreeCommandBuffers(s_Device, s_CommandPool, 1, &cmd);
}

std::vector<VkCommandBuffer> VulkanCommand::AllocateCommandBuffers(u32 count) {
    std::vector<VkCommandBuffer> buffers(count);
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = s_CommandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = count;
    vkAllocateCommandBuffers(s_Device, &allocInfo, buffers.data());
    return buffers;
}

void VulkanCommand::FreeCommandBuffers(const std::vector<VkCommandBuffer>& buffers) {
    vkFreeCommandBuffers(s_Device, s_CommandPool, (u32)buffers.size(), buffers.data());
}

// ═══════════════════════════════════════════════════════════
//  Pipeline
// ═══════════════════════════════════════════════════════════

VkShaderModule VulkanPipeline::CreateShaderModule(const std::vector<u8>& code) {
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const u32*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(s_Device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] 创建 ShaderModule 失败");
        return VK_NULL_HANDLE;
    }
    return shaderModule;
}

VkPipeline VulkanPipeline::Create(const VulkanPipelineConfig& config, VkPipelineLayout& outLayout) {
    VkShaderModule vertModule = CreateShaderModule(config.VertexShaderSPIRV);
    VkShaderModule fragModule = CreateShaderModule(config.FragmentShaderSPIRV);

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName  = "main";

    // 空顶点输入 (hardcoded triangle)
    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates    = dynamicStates;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth   = 1.0f;
    rasterizer.cullMode    = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable  = config.DepthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlend = {};
    colorBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                              | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlend.blendEnable = config.Blending ? VK_TRUE : VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlend;

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    vkCreatePipelineLayout(s_Device, &layoutInfo, nullptr, &outLayout);

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
    pipelineInfo.layout              = outLayout;
    pipelineInfo.renderPass          = config.RenderPass;
    pipelineInfo.subpass             = 0;

    VkPipeline pipeline;
    VkResult result = vkCreateGraphicsPipelines(s_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    vkDestroyShaderModule(s_Device, vertModule, nullptr);
    vkDestroyShaderModule(s_Device, fragModule, nullptr);

    if (result != VK_SUCCESS) {
        LOG_ERROR("[Vulkan] 创建 Pipeline 失败");
        return VK_NULL_HANDLE;
    }
    return pipeline;
}

void VulkanPipeline::Destroy(VkPipeline pipeline, VkPipelineLayout layout) {
    vkDestroyPipeline(s_Device, pipeline, nullptr);
    vkDestroyPipelineLayout(s_Device, layout, nullptr);
}

// ═══════════════════════════════════════════════════════════
//  Renderer (帧循环)
// ═══════════════════════════════════════════════════════════

bool VulkanRenderer::CreateSyncObjects() {
    s_ImageAvailable.resize(MAX_FRAMES_IN_FLIGHT);
    s_RenderFinished.resize(MAX_FRAMES_IN_FLIGHT);
    s_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semInfo = {};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(s_Device, &semInfo, nullptr, &s_ImageAvailable[i]) != VK_SUCCESS ||
            vkCreateSemaphore(s_Device, &semInfo, nullptr, &s_RenderFinished[i]) != VK_SUCCESS ||
            vkCreateFence(s_Device, &fenceInfo, nullptr, &s_InFlightFences[i]) != VK_SUCCESS) {
            LOG_ERROR("[Vulkan] 创建同步对象失败");
            return false;
        }
    }
    return true;
}

bool VulkanRenderer::Init(const VulkanContextConfig& config) {
    if (!VulkanContext::Init(config)) return false;
    if (!VulkanSwapchain::Create(config.Width, config.Height)) return false;

    s_CommandBuffers = VulkanCommand::AllocateCommandBuffers(MAX_FRAMES_IN_FLIGHT);
    if (!CreateSyncObjects()) return false;

    LOG_INFO("[Vulkan] Renderer 初始化完成");
    return true;
}

void VulkanRenderer::Shutdown() {
    vkDeviceWaitIdle(VulkanContext::GetDevice());

    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(s_Device, s_ImageAvailable[i], nullptr);
        vkDestroySemaphore(s_Device, s_RenderFinished[i], nullptr);
        vkDestroyFence(s_Device, s_InFlightFences[i], nullptr);
    }
    s_ImageAvailable.clear();
    s_RenderFinished.clear();
    s_InFlightFences.clear();

    VulkanSwapchain::Destroy();
    VulkanContext::Shutdown();
}

// ── 全局 Clear Color ─────────────────────────────────────────
static f32 s_ClearR = 0.01f, s_ClearG = 0.01f, s_ClearB = 0.02f, s_ClearA = 1.0f;

void VulkanRenderer::SetClearColor(f32 r, f32 g, f32 b, f32 a) {
    s_ClearR = r; s_ClearG = g; s_ClearB = b; s_ClearA = a;
}

void VulkanRenderer::BeginFrame() {
    vkWaitForFences(s_Device, 1, &s_InFlightFences[s_CurrentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(s_Device, 1, &s_InFlightFences[s_CurrentFrame]);

    s_CurrentImage = VulkanSwapchain::AcquireNextImage(s_ImageAvailable[s_CurrentFrame]);
    if (s_CurrentImage == UINT32_MAX) {
        // swapchain 失效，跳过本帧
        return;
    }

    VkCommandBuffer cmd = s_CommandBuffers[s_CurrentFrame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkClearValue clearValues[2] = {};
    clearValues[0].color = {{s_ClearR, s_ClearG, s_ClearB, s_ClearA}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rpInfo = {};
    rpInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass        = VulkanSwapchain::GetRenderPass();
    rpInfo.framebuffer       = VulkanSwapchain::GetFramebuffer(s_CurrentImage);
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = VulkanSwapchain::GetExtent();
    rpInfo.clearValueCount   = 2;
    rpInfo.pClearValues      = clearValues;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {};
    viewport.x      = 0.0f;
    viewport.y      = 0.0f;
    viewport.width  = (f32)s_SwapExtent.width;
    viewport.height = (f32)s_SwapExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {{0, 0}, s_SwapExtent};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void VulkanRenderer::EndFrame() {
    VkCommandBuffer cmd = s_CommandBuffers[s_CurrentFrame];
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    VkSemaphore waitSemaphores[]   = {s_ImageAvailable[s_CurrentFrame]};
    VkSemaphore signalSemaphores[] = {s_RenderFinished[s_CurrentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submitInfo = {};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = waitSemaphores;
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSemaphores;

    vkQueueSubmit(s_GraphicsQueue, 1, &submitInfo, s_InFlightFences[s_CurrentFrame]);
    VulkanSwapchain::Present(s_CurrentImage, s_RenderFinished[s_CurrentFrame]);

    s_CurrentFrame = (s_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

bool VulkanRenderer::ShouldRecreateSwapchain() { return s_NeedResize; }

void VulkanRenderer::OnResize(u32 width, u32 height) {
    // 窗口最小化时 extent 为 (0,0)，跳过 Recreate 等待恢复
    if (width == 0 || height == 0) {
        LOG_DEBUG("[Vulkan] 窗口最小化，跳过 Swapchain 重建");
        return;
    }
    vkDeviceWaitIdle(VulkanContext::GetDevice());
    VulkanSwapchain::Recreate(width, height);
    s_NeedResize = false;
}

VkCommandBuffer VulkanRenderer::GetCurrentCommandBuffer() { return s_CommandBuffers[s_CurrentFrame]; }
u32 VulkanRenderer::GetCurrentFrameIndex() { return s_CurrentFrame; }

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
