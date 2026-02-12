#pragma once

#include "engine/core/types.h"
#include "engine/renderer/mesh.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Engine {

// ── glTF PBR 材质信息 ───────────────────────────────────────

struct GltfMaterial {
    glm::vec4 BaseColor = {1, 1, 1, 1};
    f32 Metallic  = 0.0f;
    f32 Roughness = 1.0f;
    f32 Emissive  = 0.0f;
    std::string Name = "Default";
};

// ── glTF 网格 + 材质 ───────────────────────────────────────

struct GltfMesh {
    Scope<Mesh> MeshData;
    GltfMaterial Material;
    std::string Name;
};

// ── glTF 加载器 ────────────────────────────────────────────

class GltfLoader {
public:
    /// 从 .gltf / .glb 文件加载所有网格
    static std::vector<GltfMesh> Load(const std::string& filepath);
};

} // namespace Engine
