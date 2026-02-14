#include "engine/renderer/vulkan/vulkan_mesh.h"

#ifdef ENGINE_ENABLE_VULKAN

#include "engine/renderer/vulkan/vulkan_context.h"
#include "engine/renderer/mesh.h"
#include "engine/core/log.h"

#include <glm/glm.hpp>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Engine {

// ── 构造：Staging → Device Local ───────────────────────────

VulkanMesh::VulkanMesh(const std::vector<MeshVertex>& vertices,
                       const std::vector<u32>& indices) {
    m_VertexCount = (u32)vertices.size();
    m_IndexCount  = (u32)indices.size();

    // ── Vertex Buffer ──────
    VkDeviceSize vbSize = sizeof(MeshVertex) * m_VertexCount;

    VkBuffer stagingVB;
    VkDeviceMemory stagingVBMem;
    VulkanBuffer::CreateBuffer(vbSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingVB, stagingVBMem);

    void* data;
    vkMapMemory(VulkanContext::GetDevice(), stagingVBMem, 0, vbSize, 0, &data);
    std::memcpy(data, vertices.data(), vbSize);
    vkUnmapMemory(VulkanContext::GetDevice(), stagingVBMem);

    VulkanBuffer::CreateBuffer(vbSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_VertexBuffer, m_VertexMemory);

    VulkanBuffer::CopyBuffer(stagingVB, m_VertexBuffer, vbSize);
    VulkanBuffer::DestroyBuffer(stagingVB, stagingVBMem);

    // ── Index Buffer ───────
    VkDeviceSize ibSize = sizeof(u32) * m_IndexCount;

    VkBuffer stagingIB;
    VkDeviceMemory stagingIBMem;
    VulkanBuffer::CreateBuffer(ibSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingIB, stagingIBMem);

    vkMapMemory(VulkanContext::GetDevice(), stagingIBMem, 0, ibSize, 0, &data);
    std::memcpy(data, indices.data(), ibSize);
    vkUnmapMemory(VulkanContext::GetDevice(), stagingIBMem);

    VulkanBuffer::CreateBuffer(ibSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_IndexBuffer, m_IndexMemory);

    VulkanBuffer::CopyBuffer(stagingIB, m_IndexBuffer, ibSize);
    VulkanBuffer::DestroyBuffer(stagingIB, stagingIBMem);

    LOG_DEBUG("[Vulkan] Mesh 已创建: %u 顶点, %u 索引", m_VertexCount, m_IndexCount);
}

VulkanMesh::~VulkanMesh() {
    VulkanBuffer::DestroyBuffer(m_IndexBuffer, m_IndexMemory);
    VulkanBuffer::DestroyBuffer(m_VertexBuffer, m_VertexMemory);
}

void VulkanMesh::Bind(VkCommandBuffer cmd) const {
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &m_VertexBuffer, &offset);
    vkCmdBindIndexBuffer(cmd, m_IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void VulkanMesh::Draw(VkCommandBuffer cmd) const {
    vkCmdDrawIndexed(cmd, m_IndexCount, 1, 0, 0, 0);
}

// ═══════════════════════════════════════════════════════════
//  预置几何体
// ═══════════════════════════════════════════════════════════

std::unique_ptr<VulkanMesh> VulkanMesh::CreateCube() {
    float s = 0.5f;
    std::vector<MeshVertex> v = {
        // Front (+Z)
        {{-s,-s, s}, {0,0,1}, {0,0}}, {{ s,-s, s}, {0,0,1}, {1,0}},
        {{ s, s, s}, {0,0,1}, {1,1}}, {{-s, s, s}, {0,0,1}, {0,1}},
        // Back (-Z)
        {{ s,-s,-s}, {0,0,-1}, {0,0}}, {{-s,-s,-s}, {0,0,-1}, {1,0}},
        {{-s, s,-s}, {0,0,-1}, {1,1}}, {{ s, s,-s}, {0,0,-1}, {0,1}},
        // Top (+Y)
        {{-s, s, s}, {0,1,0}, {0,0}}, {{ s, s, s}, {0,1,0}, {1,0}},
        {{ s, s,-s}, {0,1,0}, {1,1}}, {{-s, s,-s}, {0,1,0}, {0,1}},
        // Bottom (-Y)
        {{-s,-s,-s}, {0,-1,0}, {0,0}}, {{ s,-s,-s}, {0,-1,0}, {1,0}},
        {{ s,-s, s}, {0,-1,0}, {1,1}}, {{-s,-s, s}, {0,-1,0}, {0,1}},
        // Right (+X)
        {{ s,-s, s}, {1,0,0}, {0,0}}, {{ s,-s,-s}, {1,0,0}, {1,0}},
        {{ s, s,-s}, {1,0,0}, {1,1}}, {{ s, s, s}, {1,0,0}, {0,1}},
        // Left (-X)
        {{-s,-s,-s}, {-1,0,0}, {0,0}}, {{-s,-s, s}, {-1,0,0}, {1,0}},
        {{-s, s, s}, {-1,0,0}, {1,1}}, {{-s, s,-s}, {-1,0,0}, {0,1}},
    };
    std::vector<u32> idx;
    for (u32 face = 0; face < 6; face++) {
        u32 b = face * 4;
        idx.insert(idx.end(), {b,b+1,b+2, b,b+2,b+3});
    }
    return std::make_unique<VulkanMesh>(v, idx);
}

std::unique_ptr<VulkanMesh> VulkanMesh::CreatePlane(f32 size, f32 uvScale) {
    f32 h = size * 0.5f;
    std::vector<MeshVertex> v = {
        {{-h, 0,  h}, {0,1,0}, {0,        0}},
        {{ h, 0,  h}, {0,1,0}, {uvScale,  0}},
        {{ h, 0, -h}, {0,1,0}, {uvScale,  uvScale}},
        {{-h, 0, -h}, {0,1,0}, {0,        uvScale}},
    };
    std::vector<u32> idx = {0,1,2, 0,2,3};
    return std::make_unique<VulkanMesh>(v, idx);
}

std::unique_ptr<VulkanMesh> VulkanMesh::CreateSphere(u32 stacks, u32 slices) {
    std::vector<MeshVertex> v;
    std::vector<u32> idx;

    for (u32 i = 0; i <= stacks; i++) {
        f32 phi = (f32)M_PI * (f32)i / (f32)stacks;
        for (u32 j = 0; j <= slices; j++) {
            f32 theta = 2.0f * (f32)M_PI * (f32)j / (f32)slices;
            glm::vec3 n = {
                sinf(phi) * cosf(theta),
                cosf(phi),
                sinf(phi) * sinf(theta)
            };
            MeshVertex mv;
            mv.Position = n * 0.5f;
            mv.Normal   = n;
            mv.TexCoord = {(f32)j / slices, (f32)i / stacks};
            v.push_back(mv);
        }
    }

    for (u32 i = 0; i < stacks; i++) {
        for (u32 j = 0; j < slices; j++) {
            u32 a = i * (slices + 1) + j;
            u32 b = a + slices + 1;
            idx.insert(idx.end(), {a, b, a+1, a+1, b, b+1});
        }
    }
    return std::make_unique<VulkanMesh>(v, idx);
}

} // namespace Engine

#endif // ENGINE_ENABLE_VULKAN
