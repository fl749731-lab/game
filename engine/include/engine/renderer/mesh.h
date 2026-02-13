#pragma once

#include "engine/core/types.h"
#include "engine/renderer/buffer.h"

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Engine {

// ── 网格顶点 ────────────────────────────────────────────────

struct MeshVertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoord;
    glm::vec3 Tangent  = {1, 0, 0};
    glm::vec3 Bitangent = {0, 0, 1};
};

// ── 网格 ────────────────────────────────────────────────────

class Mesh {
public:
    /// 从顶点/索引数据构建
    Mesh(const std::vector<MeshVertex>& vertices, const std::vector<u32>& indices);
    
    /// 从 OBJ 文件加载
    static Scope<Mesh> LoadOBJ(const std::string& filepath);

    /// 预置几何体
    static Scope<Mesh> CreateCube();
    static Scope<Mesh> CreatePlane(f32 size = 10.0f, f32 uvScale = 5.0f);
    static Scope<Mesh> CreateSphere(u32 stacks = 20, u32 slices = 20);

    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    /// 移动语义 (允许容器使用)
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void Draw() const;

    u32 GetVertexCount() const { return (u32)m_Vertices.size(); }
    u32 GetIndexCount() const { return m_IndexCount; }
    u32 GetVAO() const { return m_VAO; }

private:
    void SetupBuffers();

    std::vector<MeshVertex> m_Vertices;
    u32 m_IndexCount = 0;

    u32 m_VAO = 0;
    u32 m_VBO = 0;
    u32 m_IBO = 0;
};

} // namespace Engine
