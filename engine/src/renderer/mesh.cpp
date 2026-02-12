#include "engine/renderer/mesh.h"
#include "engine/renderer/renderer.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Engine {

// ── 辅助：计算切线/副切线 ────────────────────────────────────

static void CalcTangents(std::vector<MeshVertex>& vertices,
                         const std::vector<u32>& indices) {
    // 先清零
    for (auto& v : vertices) {
        v.Tangent = {0, 0, 0};
        v.Bitangent = {0, 0, 0};
    }
    // 遍历三角面，累加切线
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        auto& v0 = vertices[indices[i]];
        auto& v1 = vertices[indices[i+1]];
        auto& v2 = vertices[indices[i+2]];

        glm::vec3 e1 = v1.Position - v0.Position;
        glm::vec3 e2 = v2.Position - v0.Position;
        glm::vec2 dUV1 = v1.TexCoord - v0.TexCoord;
        glm::vec2 dUV2 = v2.TexCoord - v0.TexCoord;

        f32 det = dUV1.x * dUV2.y - dUV2.x * dUV1.y;
        if (std::abs(det) < 1e-8f) continue;
        f32 r = 1.0f / det;

        glm::vec3 tangent   = (e1 * dUV2.y - e2 * dUV1.y) * r;
        glm::vec3 bitangent = (e2 * dUV1.x - e1 * dUV2.x) * r;

        v0.Tangent += tangent; v1.Tangent += tangent; v2.Tangent += tangent;
        v0.Bitangent += bitangent; v1.Bitangent += bitangent; v2.Bitangent += bitangent;
    }
    // 归一化
    for (auto& v : vertices) {
        if (glm::length(v.Tangent) > 1e-6f)
            v.Tangent = glm::normalize(v.Tangent);
        else
            v.Tangent = {1, 0, 0};
        if (glm::length(v.Bitangent) > 1e-6f)
            v.Bitangent = glm::normalize(v.Bitangent);
        else
            v.Bitangent = {0, 0, 1};
    }
}

// ── 构造 / 析构 ─────────────────────────────────────────────

Mesh::Mesh(const std::vector<MeshVertex>& vertices, const std::vector<u32>& indices)
    : m_Vertices(vertices), m_IndexCount((u32)indices.size())
{
    glGenVertexArrays(1, &m_VAO);
    glBindVertexArray(m_VAO);

    glGenBuffers(1, &m_VBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(MeshVertex),
                 vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &m_IBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(u32),
                 indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                          (void*)offsetof(MeshVertex, Position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                          (void*)offsetof(MeshVertex, Normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                          (void*)offsetof(MeshVertex, TexCoord));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                          (void*)offsetof(MeshVertex, Tangent));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                          (void*)offsetof(MeshVertex, Bitangent));

    glBindVertexArray(0);
}

Mesh::~Mesh() {
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_IBO) glDeleteBuffers(1, &m_IBO);
}

void Mesh::Draw() const {
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, m_IndexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    // 更新渲染统计
    Renderer::NotifyDraw(m_IndexCount / 3);
}

// ── OBJ 加载器 ──────────────────────────────────────────────

static void ParseFaceToken(const std::string& token, int& vi, int& vti, int& vni) {
    vi = vti = vni = 0;
    // 格式: v, v/vt, v/vt/vn, v//vn
    size_t p1 = token.find('/');
    if (p1 == std::string::npos) {
        vi = std::stoi(token);
        return;
    }
    vi = std::stoi(token.substr(0, p1));
    size_t p2 = token.find('/', p1 + 1);
    if (p2 == std::string::npos) {
        vti = std::stoi(token.substr(p1 + 1));
        return;
    }
    std::string vtStr = token.substr(p1 + 1, p2 - p1 - 1);
    if (!vtStr.empty()) vti = std::stoi(vtStr);
    std::string vnStr = token.substr(p2 + 1);
    if (!vnStr.empty()) vni = std::stoi(vnStr);
}

Scope<Mesh> Mesh::LoadOBJ(const std::string& filepath) {
    LOG_INFO("加载 OBJ: %s", filepath.c_str());

    std::ifstream file(filepath);
    if (!file.is_open()) {
        LOG_ERROR("无法打开: %s", filepath.c_str());
        return nullptr;
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    std::vector<MeshVertex> vertices;
    std::vector<u32> indices;
    std::unordered_map<std::string, u32> uniqueVerts;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            glm::vec3 p; iss >> p.x >> p.y >> p.z;
            positions.push_back(p);
        } else if (prefix == "vn") {
            glm::vec3 n; iss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (prefix == "vt") {
            glm::vec2 t; iss >> t.x >> t.y;
            texcoords.push_back(t);
        } else if (prefix == "f") {
            std::vector<std::string> tokens;
            std::string tok;
            while (iss >> tok) tokens.push_back(tok);

            // 三角化 (fan)
            for (size_t i = 1; i + 1 < tokens.size(); i++) {
                std::string tri[3] = { tokens[0], tokens[i], tokens[i+1] };
                for (auto& key : tri) {
                    auto it = uniqueVerts.find(key);
                    if (it != uniqueVerts.end()) {
                        indices.push_back(it->second);
                    } else {
                        int vi, vti, vni;
                        ParseFaceToken(key, vi, vti, vni);

                        MeshVertex mv{};
                        if (vi > 0 && vi <= (int)positions.size())
                            mv.Position = positions[vi - 1];
                        if (vti > 0 && vti <= (int)texcoords.size())
                            mv.TexCoord = texcoords[vti - 1];
                        if (vni > 0 && vni <= (int)normals.size())
                            mv.Normal = normals[vni - 1];

                        u32 idx = (u32)vertices.size();
                        vertices.push_back(mv);
                        uniqueVerts[key] = idx;
                        indices.push_back(idx);
                    }
                }
            }
        }
    }

    // 如果没有法线，计算面法线
    if (normals.empty() && indices.size() >= 3) {
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            auto& v0 = vertices[indices[i]];
            auto& v1 = vertices[indices[i+1]];
            auto& v2 = vertices[indices[i+2]];
            glm::vec3 n = glm::normalize(glm::cross(
                v1.Position - v0.Position,
                v2.Position - v0.Position));
            v0.Normal = v1.Normal = v2.Normal = n;
        }
    }

    // 计算切线
    CalcTangents(vertices, indices);

    LOG_INFO("OBJ 完成: %zu 顶点, %zu 索引", vertices.size(), indices.size());
    return std::make_unique<Mesh>(vertices, indices);
}

// ── 预置几何体 ──────────────────────────────────────────────

Scope<Mesh> Mesh::CreateCube() {
    std::vector<MeshVertex> v;
    auto face = [&](glm::vec3 n, glm::vec3 u, glm::vec3 r) {
        glm::vec3 c = n * 0.5f;
        // tangent = r 方向, bitangent = u 方向
        v.push_back({c - u*0.5f - r*0.5f, n, {0,0}, r, u});
        v.push_back({c - u*0.5f + r*0.5f, n, {1,0}, r, u});
        v.push_back({c + u*0.5f + r*0.5f, n, {1,1}, r, u});
        v.push_back({c + u*0.5f - r*0.5f, n, {0,1}, r, u});
    };
    face({ 0, 0, 1}, {0,1,0}, {1,0,0});
    face({ 0, 0,-1}, {0,1,0}, {-1,0,0});
    face({-1, 0, 0}, {0,1,0}, {0,0,1});
    face({ 1, 0, 0}, {0,1,0}, {0,0,-1});
    face({ 0, 1, 0}, {0,0,-1},{1,0,0});
    face({ 0,-1, 0}, {0,0,1}, {1,0,0});

    std::vector<u32> idx;
    for (u32 f = 0; f < 6; f++) {
        u32 b = f * 4;
        idx.insert(idx.end(), {b,b+1,b+2, b+2,b+3,b});
    }
    return std::make_unique<Mesh>(v, idx);
}

Scope<Mesh> Mesh::CreatePlane(f32 size, f32 uvScale) {
    f32 h = size * 0.5f;
    glm::vec3 T = {1, 0, 0};
    glm::vec3 B = {0, 0, 1};
    std::vector<MeshVertex> v = {
        {{-h, 0, -h}, {0,1,0}, {0,       0},       T, B},
        {{ h, 0, -h}, {0,1,0}, {uvScale, 0},       T, B},
        {{ h, 0,  h}, {0,1,0}, {uvScale, uvScale}, T, B},
        {{-h, 0,  h}, {0,1,0}, {0,       uvScale}, T, B},
    };
    std::vector<u32> idx = {0,1,2, 2,3,0};
    return std::make_unique<Mesh>(v, idx);
}

Scope<Mesh> Mesh::CreateSphere(u32 stacks, u32 slices) {
    std::vector<MeshVertex> v;
    std::vector<u32> idx;

    for (u32 i = 0; i <= stacks; i++) {
        f32 phi = (f32)M_PI * (f32)i / (f32)stacks;
        for (u32 j = 0; j <= slices; j++) {
            f32 theta = 2.0f * (f32)M_PI * (f32)j / (f32)slices;
            glm::vec3 pos(
                sin(phi) * cos(theta),
                cos(phi),
                sin(phi) * sin(theta)
            );
            // 切线 = d(pos)/d(theta)
            glm::vec3 tangent(
                -sin(theta),
                0.0f,
                cos(theta)
            );
            glm::vec3 bitangent = glm::normalize(glm::cross(pos, tangent));

            MeshVertex mv;
            mv.Position  = pos * 0.5f;
            mv.Normal    = pos;
            mv.TexCoord  = {(f32)j / slices, (f32)i / stacks};
            mv.Tangent   = tangent;
            mv.Bitangent = bitangent;
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

    return std::make_unique<Mesh>(v, idx);
}

} // namespace Engine
