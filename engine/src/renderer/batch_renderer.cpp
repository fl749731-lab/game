#include "engine/renderer/batch_renderer.h"
#include "engine/core/resource_manager.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>

namespace Engine {

// ── 静态成员 ────────────────────────────────────────────────

u32 BatchRenderer::s_InstanceVBO = 0;
u32 BatchRenderer::s_MaxInstances = 0;
Shader* BatchRenderer::s_CurrentShader = nullptr;
std::unordered_map<BatchKey, BatchRenderer::BatchGroup, BatchKeyHash> BatchRenderer::s_Batches;
u32 BatchRenderer::s_DrawCalls = 0;
u32 BatchRenderer::s_TotalInstances = 0;

// ── 初始化 ──────────────────────────────────────────────────

void BatchRenderer::Init(u32 maxInstances) {
    s_MaxInstances = maxInstances;

    glGenBuffers(1, &s_InstanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, s_InstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(BatchInstanceData),
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    LOG_INFO("[BatchRenderer] 初始化完成 (最大 %u 实例)", maxInstances);
}

void BatchRenderer::Shutdown() {
    if (s_InstanceVBO) { glDeleteBuffers(1, &s_InstanceVBO); s_InstanceVBO = 0; }
    s_Batches.clear();
    s_CurrentShader = nullptr;
}

// ── 批次管理 ────────────────────────────────────────────────

void BatchRenderer::Begin(Shader* shader) {
    s_CurrentShader = shader;

    // 定期清理空批次（避免残留的 BatchGroup 占内存）
    static u32 s_FrameCounter = 0;
    if ((++s_FrameCounter % 120) == 0) {
        for (auto it = s_Batches.begin(); it != s_Batches.end(); ) {
            if (it->second.Instances.empty()) {
                it = s_Batches.erase(it);
            } else {
                ++it;
            }
        }
    }

    for (auto& [key, group] : s_Batches) {
        group.Instances.clear();
    }
}

void BatchRenderer::Submit(const std::string& meshType,
                           const std::string& textureName,
                           const std::string& normalMapName,
                           const BatchInstanceData& data) {
    BatchKey key{meshType, textureName, normalMapName};
    s_Batches[key].Instances.push_back(data);
}

// ── 刷新绘制 ────────────────────────────────────────────────

void BatchRenderer::End() {
    if (!s_CurrentShader) return;

    s_CurrentShader->Bind();

    for (auto& [key, group] : s_Batches) {
        if (group.Instances.empty()) continue;

        // 获取对应 Mesh
        auto* mesh = ResourceManager::GetMesh(key.MeshType);
        if (!mesh) continue;

        // 绑定纹理 (整个批次共享)
        if (!key.TextureName.empty()) {
            auto tex = ResourceManager::GetTexture(key.TextureName);
            if (tex) { tex->Bind(0); s_CurrentShader->SetInt("uTex", 0); }
        }

        if (!key.NormalMapName.empty()) {
            auto nmap = ResourceManager::GetTexture(key.NormalMapName);
            if (nmap) { nmap->Bind(2); s_CurrentShader->SetInt("uNormalMap", 2); }
        }

        // 分块上传（避免超过 GPU 缓冲区大小）
        u32 totalInstances = (u32)group.Instances.size();
        u32 offset = 0;

        while (offset < totalInstances) {
            u32 batchSize = std::min(totalInstances - offset, s_MaxInstances);

            // 上传实例数据
            glBindBuffer(GL_ARRAY_BUFFER, s_InstanceVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0,
                            batchSize * sizeof(BatchInstanceData),
                            &group.Instances[offset]);

            // 设置 VAO 实例化属性
            u32 vao = mesh->GetVAO();
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, s_InstanceVBO);

            constexpr u32 BASE = INSTANCE_ATTRIB_START; // = 5

            // Model 矩阵 → location 5~8 (mat4 = 4 × vec4)
            for (int i = 0; i < 4; i++) {
                u32 loc = BASE + i;
                glEnableVertexAttribArray(loc);
                glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE,
                                      sizeof(BatchInstanceData),
                                      (void*)(sizeof(glm::vec4) * i));
                glVertexAttribDivisor(loc, 1);
            }

            // Albedo (rgb+metallic) → location 9
            glEnableVertexAttribArray(BASE + 4);
            glVertexAttribPointer(BASE + 4, 4, GL_FLOAT, GL_FALSE,
                                  sizeof(BatchInstanceData),
                                  (void*)(offsetof(BatchInstanceData, Albedo)));
            glVertexAttribDivisor(BASE + 4, 1);

            // EmissiveInfo → location 10
            glEnableVertexAttribArray(BASE + 5);
            glVertexAttribPointer(BASE + 5, 4, GL_FLOAT, GL_FALSE,
                                  sizeof(BatchInstanceData),
                                  (void*)(offsetof(BatchInstanceData, EmissiveInfo)));
            glVertexAttribDivisor(BASE + 5, 1);

            // MaterialParams → location 11
            glEnableVertexAttribArray(BASE + 6);
            glVertexAttribPointer(BASE + 6, 4, GL_FLOAT, GL_FALSE,
                                  sizeof(BatchInstanceData),
                                  (void*)(offsetof(BatchInstanceData, MaterialParams)));
            glVertexAttribDivisor(BASE + 6, 1);

            // 实例化绘制 (使用索引缓冲)
            glDrawElementsInstanced(GL_TRIANGLES,
                                    mesh->GetIndexCount(),
                                    GL_UNSIGNED_INT, nullptr,
                                    (GLsizei)batchSize);

            // 清理分频器
            for (u32 loc = BASE; loc <= BASE + 6; loc++) {
                glVertexAttribDivisor(loc, 0);
                glDisableVertexAttribArray(loc);
            }

            glBindVertexArray(0);

            s_DrawCalls++;
            s_TotalInstances += batchSize;
            offset += batchSize;
        }
    }
}

void BatchRenderer::ResetStats() {
    s_DrawCalls = 0;
    s_TotalInstances = 0;
}

} // namespace Engine
