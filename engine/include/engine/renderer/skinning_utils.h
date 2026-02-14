#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"

#include <glm/glm.hpp>

#include <vector>

namespace Engine {

// ── GPU 骨骼蒙皮辅助 ────────────────────────────────────────
// 提供骨骼矩阵 uniform 上传和蒙皮网格 VAO 布局设置

class SkinningUtils {
public:
    /// 上传骨骼矩阵到 shader 的 uBones[] uniform 数组
    static void UploadBoneMatrices(Shader* shader,
                                   const std::vector<glm::mat4>& boneMatrices);

    /// 设置蒙皮网格的顶点属性布局
    /// 在常规 VAO 设置之后调用，为 aBoneIDs (loc=5) 和 aWeights (loc=6) 配置属性
    /// @param boneIDOffset   骨骼ID数据在顶点结构中的字节偏移
    /// @param weightOffset   权重数据在顶点结构中的字节偏移
    /// @param stride         顶点结构总字节大小
    static void SetupSkinningAttributes(u32 boneIDOffset, u32 weightOffset, u32 stride);

    /// 蒙皮顶点结构 (用于 GPU skinning 流水线)
    struct SkinVertex {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 UV;
        glm::vec3 Tangent;
        glm::vec3 Bitangent;
        i32       BoneIDs[4]  = {-1, -1, -1, -1};
        f32       Weights[4]  = {0.0f, 0.0f, 0.0f, 0.0f};
    };

    /// 一键设置 SkinVertex 结构的完整 VAO 属性
    static void SetupSkinVertexVAO();
};

} // namespace Engine
