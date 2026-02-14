#include "engine/rhi/rhi_device.h"
#include "engine/rhi/opengl/gl_device.h"
#include "engine/core/log.h"

#ifdef ENGINE_ENABLE_VULKAN
#include "engine/rhi/vulkan/vk_device.h"
#endif

namespace Engine {

Scope<RHIDevice> RHIDevice::Create(GraphicsBackend backend) {
    switch (backend) {
        case GraphicsBackend::OpenGL:
            LOG_INFO("[RHI] 创建 OpenGL 渲染设备");
            return CreateScope<GLDevice>();

        case GraphicsBackend::Vulkan:
#ifdef ENGINE_ENABLE_VULKAN
            LOG_INFO("[RHI] 创建 Vulkan 渲染设备");
            return CreateScope<VKDevice>();
#else
            LOG_ERROR("[RHI] Vulkan 后端未启用! 请使用 -DENGINE_ENABLE_VULKAN=ON 编译");
            return nullptr;
#endif
    }

    LOG_ERROR("[RHI] 未知的后端类型");
    return nullptr;
}

} // namespace Engine
