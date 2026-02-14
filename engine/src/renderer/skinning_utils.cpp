#include "engine/renderer/skinning_utils.h"
#include "engine/renderer/animation.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace Engine {

// ── 上传骨骼矩阵 ────────────────────────────────────────────

void SkinningUtils::UploadBoneMatrices(Shader* shader,
                                        const std::vector<glm::mat4>& boneMatrices) {
    if (!shader || boneMatrices.empty()) return;

    // 上限检查
    u32 count = (u32)boneMatrices.size();
    if (count > MAX_BONES) {
        LOG_WARN("[Skinning] 骨骼数 %u 超过上限 %u, 截断", count, MAX_BONES);
        count = MAX_BONES;
    }

    // 逐个上传 (避免一次性大数组 uniform 的兼容性问题)
    for (u32 i = 0; i < count; i++) {
        std::string name = "uBones[" + std::to_string(i) + "]";
        shader->SetMat4(name, glm::value_ptr(boneMatrices[i]));
    }
}

// ── 设置蒙皮顶点属性 ────────────────────────────────────────

void SkinningUtils::SetupSkinningAttributes(u32 boneIDOffset, u32 weightOffset, u32 stride) {
    // location = 5: aBoneIDs (ivec4, integer attribute)
    glEnableVertexAttribArray(5);
    glVertexAttribIPointer(5, 4, GL_INT, stride, (void*)(uintptr_t)boneIDOffset);

    // location = 6: aWeights (vec4, float attribute)
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride, (void*)(uintptr_t)weightOffset);
}

// ── SkinVertex 完整 VAO 设置 ────────────────────────────────

void SkinningUtils::SetupSkinVertexVAO() {
    u32 stride = sizeof(SkinVertex);

    // location = 0: aPos (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
        (void*)offsetof(SkinVertex, Position));

    // location = 1: aNormal (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
        (void*)offsetof(SkinVertex, Normal));

    // location = 2: aUV (vec2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
        (void*)offsetof(SkinVertex, UV));

    // location = 3: aTangent (vec3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride,
        (void*)offsetof(SkinVertex, Tangent));

    // location = 4: aBitangent (vec3)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride,
        (void*)offsetof(SkinVertex, Bitangent));

    // location = 5: aBoneIDs (ivec4) — 整型属性
    glEnableVertexAttribArray(5);
    glVertexAttribIPointer(5, 4, GL_INT, stride,
        (void*)offsetof(SkinVertex, BoneIDs));

    // location = 6: aWeights (vec4)
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride,
        (void*)offsetof(SkinVertex, Weights));
}

} // namespace Engine
