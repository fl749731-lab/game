#pragma once

#include "engine/core/types.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/animation.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

namespace Engine {

// ── glTF 蒙皮顶点 ──────────────────────────────────────────

struct GltfSkinVertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoord;
    glm::vec3 Tangent  = {1, 0, 0};
    glm::vec3 Bitangent = {0, 0, 1};
    glm::ivec4 BoneIDs  = {0, 0, 0, 0};   // 最多 4 根骨骼
    glm::vec4  Weights  = {0, 0, 0, 0};   // 对应权重
};

// ── glTF PBR 材质信息 ───────────────────────────────────────

struct GltfMaterial {
    glm::vec4 BaseColor = {1, 1, 1, 1};
    f32 Metallic  = 0.0f;
    f32 Roughness = 1.0f;
    f32 Emissive  = 0.0f;
    glm::vec3 EmissiveColor = {0, 0, 0};
    std::string Name = "Default";
    // 纹理路径 (空=无纹理)
    std::string BaseColorTexPath;
    std::string NormalTexPath;
    std::string MetallicRoughnessTexPath;
};

// ── glTF 网格 + 材质 ───────────────────────────────────────

struct GltfMesh {
    Scope<Mesh> MeshData;
    GltfMaterial Material;
    std::string Name;

    // 蒙皮数据 (可选)
    bool HasSkin = false;
    std::vector<GltfSkinVertex> SkinVertices;
    Ref<Skeleton> SkeletonRef;
    std::vector<AnimationClip> Clips;
};

// ── glTF 加载器 ────────────────────────────────────────────

class GltfLoader {
public:
    /// 从 .gltf / .glb 文件加载所有网格
    static std::vector<GltfMesh> Load(const std::string& filepath);
};

} // namespace Engine
