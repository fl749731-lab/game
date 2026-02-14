#pragma once

#include "engine/core/types.h"
#include "engine/core/log.h"

namespace Engine {

// ── Vulkan 后端骨架 ────────────────────────────────────────
//
// 当前仅提供接口定义和初始化骨架
// 后续将逐步实现 Pipeline/Buffer/Command 等子系统
// 需要 cmake -DENGINE_ENABLE_VULKAN=ON 开启

#ifdef ENGINE_ENABLE_VULKAN

// ── Vulkan Context ─────────────────────────────────────────

struct VulkanContextConfig {
    const char* AppName     = "Engine App";
    u32         ApiVersion  = 0;  // 0 = 自动选择
    bool        Validation  = true;
    u32         Width       = 1280;
    u32         Height      = 720;
};

class VulkanContext {
public:
    static bool Init(const VulkanContextConfig& config);
    static void Shutdown();
    static bool IsInitialized();

    // 获取原始 Vulkan 句柄 (void* 以避免头文件依赖)
    static void* GetInstance();
    static void* GetPhysicalDevice();
    static void* GetDevice();
    static void* GetGraphicsQueue();
    static u32   GetGraphicsQueueFamily();
    static void* GetSurface();

private:
    static bool CreateInstance(const VulkanContextConfig& config);
    static bool SelectPhysicalDevice();
    static bool CreateLogicalDevice();
};

// ── Vulkan Pipeline ────────────────────────────────────────

struct VulkanPipelineConfig {
    const char* VertexShaderPath   = nullptr;
    const char* FragmentShaderPath = nullptr;
    bool        DepthTest          = true;
    bool        Blending           = false;
    // 扩展: 顶点输入布局、推送常量等
};

class VulkanPipeline {
public:
    static void* Create(const VulkanPipelineConfig& config);
    static void  Destroy(void* pipeline);
};

// ── Vulkan Buffer ──────────────────────────────────────────

enum class VulkanBufferType : u8 {
    Vertex,
    Index,
    Uniform,
    Storage,
    Staging
};

class VulkanBuffer {
public:
    static void* Create(VulkanBufferType type, u64 size, const void* data = nullptr);
    static void  Destroy(void* buffer);
    static void  Upload(void* buffer, const void* data, u64 size, u64 offset = 0);
    static void* Map(void* buffer);
    static void  Unmap(void* buffer);
};

// ── Vulkan Command ─────────────────────────────────────────

class VulkanCommand {
public:
    static void* AllocateCommandBuffer();
    static void  FreeCommandBuffer(void* cmdBuffer);
    static void  Begin(void* cmdBuffer);
    static void  End(void* cmdBuffer);
    static void  Submit(void* cmdBuffer, bool waitIdle = false);
};

// ── Vulkan Shader (SPIR-V) ─────────────────────────────────

class VulkanShader {
public:
    static void* LoadFromSPIRV(const char* vertPath, const char* fragPath);
    static void* LoadFromGLSL(const char* vertSrc, const char* fragSrc);
    static void  Destroy(void* shaderModule);
};

// ── Vulkan Swapchain ───────────────────────────────────────

class VulkanSwapchain {
public:
    static bool Create(u32 width, u32 height);
    static void Destroy();
    static void Resize(u32 width, u32 height);
    static u32  AcquireNextImage();
    static void Present();
    static u32  GetImageCount();
};

#else // !ENGINE_ENABLE_VULKAN

// 无 Vulkan 时的空占位
class VulkanContext {
public:
    static bool Init(const void*) {
        LOG_WARN("[Vulkan] Vulkan backend disabled. Rebuild with -DENGINE_ENABLE_VULKAN=ON");
        return false;
    }
    static void Shutdown() {}
    static bool IsInitialized() { return false; }
};

#endif // ENGINE_ENABLE_VULKAN

} // namespace Engine
