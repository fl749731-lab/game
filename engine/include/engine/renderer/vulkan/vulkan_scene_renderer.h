#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include <vulkan/vulkan.h>
#include <string>

namespace Engine {

class Scene;
class PerspectiveCamera;

// ── Vulkan 场景渲染器 (前向渲染) ────────────────────────────

struct VulkanSceneConfig {
    u32 Width  = 1280;
    u32 Height = 720;
    std::string ShaderDir = "assets/shaders/vulkan/";
};

class VulkanSceneRenderer {
public:
    static bool Init(const VulkanSceneConfig& config);
    static void Shutdown();

    /// 渲染一帧
    static void RenderScene(Scene& scene, PerspectiveCamera& camera);

    static void OnResize(u32 width, u32 height);

private:
    static void CreateResources();
    static void DestroyResources();
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
