#pragma once

#include "engine/core/types.h"

#ifdef ENGINE_ENABLE_VULKAN

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace Engine {

struct MeshVertex; // 前向声明

// ── Vulkan Mesh ────────────────────────────────────────────

class VulkanMesh {
public:
    /// 从顶点+索引数据构建 (数据通过 Staging Buffer 上传到 Device Local)
    VulkanMesh(const std::vector<MeshVertex>& vertices, const std::vector<u32>& indices);
    ~VulkanMesh();

    VulkanMesh(const VulkanMesh&) = delete;
    VulkanMesh& operator=(const VulkanMesh&) = delete;

    void Bind(VkCommandBuffer cmd) const;
    void Draw(VkCommandBuffer cmd) const;

    u32 GetVertexCount() const { return m_VertexCount; }
    u32 GetIndexCount()  const { return m_IndexCount; }
    bool IsValid() const { return m_VertexBuffer != VK_NULL_HANDLE; }

    // ── 预置几何体 ──────────────────────────────────────────
    static std::unique_ptr<VulkanMesh> CreateCube();
    static std::unique_ptr<VulkanMesh> CreatePlane(f32 size = 10.0f, f32 uvScale = 5.0f);
    static std::unique_ptr<VulkanMesh> CreateSphere(u32 stacks = 20, u32 slices = 20);

private:
    VkBuffer       m_VertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_VertexMemory = VK_NULL_HANDLE;
    VkBuffer       m_IndexBuffer  = VK_NULL_HANDLE;
    VkDeviceMemory m_IndexMemory  = VK_NULL_HANDLE;
    u32 m_VertexCount = 0;
    u32 m_IndexCount  = 0;
};

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
