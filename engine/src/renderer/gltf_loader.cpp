#include "engine/renderer/gltf_loader.h"
#include "engine/core/log.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include <glm/glm.hpp>

namespace Engine {

// ── 辅助：从 accessor 读取数据 ─────────────────────────────

static const void* AccessorData(const cgltf_accessor* accessor) {
    const cgltf_buffer_view* view = accessor->buffer_view;
    return (const u8*)view->buffer->data + view->offset + accessor->offset;
}

// ── 加载 ────────────────────────────────────────────────────

std::vector<GltfMesh> GltfLoader::Load(const std::string& filepath) {
    std::vector<GltfMesh> results;

    cgltf_options options = {};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, filepath.c_str(), &data);

    if (result != cgltf_result_success) {
        LOG_ERROR("[glTF] 解析失败: %s", filepath.c_str());
        return results;
    }

    result = cgltf_load_buffers(&options, data, filepath.c_str());
    if (result != cgltf_result_success) {
        LOG_ERROR("[glTF] 缓冲区加载失败: %s", filepath.c_str());
        cgltf_free(data);
        return results;
    }

    // 提取基础目录 (用于纹理路径)
    std::string baseDir = filepath;
    auto lastSlash = baseDir.find_last_of("/\\");
    baseDir = (lastSlash != std::string::npos) ? baseDir.substr(0, lastSlash + 1) : "";

    // 遍历所有 mesh
    for (cgltf_size mi = 0; mi < data->meshes_count; mi++) {
        const cgltf_mesh& mesh = data->meshes[mi];

        for (cgltf_size pi = 0; pi < mesh.primitives_count; pi++) {
            const cgltf_primitive& prim = mesh.primitives[pi];

            if (prim.type != cgltf_primitive_type_triangles) continue;

            std::vector<MeshVertex> vertices;
            std::vector<u32> indices;

            // 读取顶点属性
            const cgltf_accessor* posAccessor  = nullptr;
            const cgltf_accessor* normAccessor = nullptr;
            const cgltf_accessor* uvAccessor   = nullptr;
            const cgltf_accessor* tanAccessor  = nullptr;

            for (cgltf_size ai = 0; ai < prim.attributes_count; ai++) {
                const auto& attr = prim.attributes[ai];
                if (attr.type == cgltf_attribute_type_position)  posAccessor  = attr.data;
                if (attr.type == cgltf_attribute_type_normal)    normAccessor = attr.data;
                if (attr.type == cgltf_attribute_type_texcoord)  uvAccessor   = attr.data;
                if (attr.type == cgltf_attribute_type_tangent)   tanAccessor  = attr.data;
            }

            if (!posAccessor) continue;

            cgltf_size vertCount = posAccessor->count;
            vertices.resize(vertCount);

            // Position
            {
                const f32* src = (const f32*)AccessorData(posAccessor);
                for (cgltf_size v = 0; v < vertCount; v++) {
                    vertices[v].Position = {src[v*3+0], src[v*3+1], src[v*3+2]};
                }
            }

            // Normal
            if (normAccessor) {
                const f32* src = (const f32*)AccessorData(normAccessor);
                for (cgltf_size v = 0; v < vertCount; v++) {
                    vertices[v].Normal = {src[v*3+0], src[v*3+1], src[v*3+2]};
                }
            } else {
                for (auto& v : vertices) v.Normal = {0, 1, 0};
            }

            // UV
            if (uvAccessor) {
                const f32* src = (const f32*)AccessorData(uvAccessor);
                for (cgltf_size v = 0; v < vertCount; v++) {
                    vertices[v].TexCoord = {src[v*2+0], src[v*2+1]};
                }
            }

            // Tangent (vec4: xyz=tangent, w=handedness)
            if (tanAccessor) {
                const f32* src = (const f32*)AccessorData(tanAccessor);
                for (cgltf_size v = 0; v < vertCount; v++) {
                    glm::vec3 T = {src[v*4+0], src[v*4+1], src[v*4+2]};
                    f32 w = src[v*4+3]; // handedness
                    vertices[v].Tangent = T;
                    vertices[v].Bitangent = glm::cross(vertices[v].Normal, T) * w;
                }
            }

            // Indices
            if (prim.indices) {
                indices.resize(prim.indices->count);
                for (cgltf_size i = 0; i < prim.indices->count; i++) {
                    indices[i] = (u32)cgltf_accessor_read_index(prim.indices, i);
                }
            } else {
                // 无索引 → 生成顺序索引
                indices.resize(vertCount);
                for (cgltf_size i = 0; i < vertCount; i++) indices[i] = (u32)i;
            }

            // 自动计算 Tangent（如果文件中没有提供）
            if (!tanAccessor && uvAccessor) {
                for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                    auto& v0 = vertices[indices[i+0]];
                    auto& v1 = vertices[indices[i+1]];
                    auto& v2 = vertices[indices[i+2]];
                    glm::vec3 e1 = v1.Position - v0.Position;
                    glm::vec3 e2 = v2.Position - v0.Position;
                    glm::vec2 dUV1 = v1.TexCoord - v0.TexCoord;
                    glm::vec2 dUV2 = v2.TexCoord - v0.TexCoord;
                    f32 f = 1.0f / (dUV1.x * dUV2.y - dUV2.x * dUV1.y + 0.0001f);
                    glm::vec3 T = glm::normalize(f * (dUV2.y * e1 - dUV1.y * e2));
                    glm::vec3 B = glm::normalize(f * (-dUV2.x * e1 + dUV1.x * e2));
                    v0.Tangent = T; v0.Bitangent = B;
                    v1.Tangent = T; v1.Bitangent = B;
                    v2.Tangent = T; v2.Bitangent = B;
                }
            }

            // 材质
            GltfMaterial mat;
            if (prim.material) {
                const cgltf_material& gmat = *prim.material;
                if (gmat.name) mat.Name = gmat.name;

                if (gmat.has_pbr_metallic_roughness) {
                    auto& pbr = gmat.pbr_metallic_roughness;
                    mat.BaseColor = {pbr.base_color_factor[0], pbr.base_color_factor[1],
                                     pbr.base_color_factor[2], pbr.base_color_factor[3]};
                    mat.Metallic  = pbr.metallic_factor;
                    mat.Roughness = pbr.roughness_factor;

                    // BaseColor 纹理路径
                    if (pbr.base_color_texture.texture &&
                        pbr.base_color_texture.texture->image &&
                        pbr.base_color_texture.texture->image->uri) {
                        mat.BaseColorTexPath = baseDir + pbr.base_color_texture.texture->image->uri;
                    }

                    // MetallicRoughness 纹理路径
                    if (pbr.metallic_roughness_texture.texture &&
                        pbr.metallic_roughness_texture.texture->image &&
                        pbr.metallic_roughness_texture.texture->image->uri) {
                        mat.MetallicRoughnessTexPath = baseDir + pbr.metallic_roughness_texture.texture->image->uri;
                    }
                }

                // 法线贴图路径
                if (gmat.normal_texture.texture &&
                    gmat.normal_texture.texture->image &&
                    gmat.normal_texture.texture->image->uri) {
                    mat.NormalTexPath = baseDir + gmat.normal_texture.texture->image->uri;
                }

                // Emissive
                f32 eFactor = gmat.emissive_factor[0] + gmat.emissive_factor[1] + gmat.emissive_factor[2];
                if (eFactor > 0.001f) {
                    mat.EmissiveColor = {gmat.emissive_factor[0], gmat.emissive_factor[1], gmat.emissive_factor[2]};
                    mat.Emissive = (gmat.has_emissive_strength)
                        ? gmat.emissive_strength.emissive_strength
                        : 1.0f;
                }
            }

            // 构建 Mesh 并加入结果
            GltfMesh gltfMesh;
            gltfMesh.MeshData = std::make_unique<Mesh>(vertices, indices);
            gltfMesh.Material = mat;
            gltfMesh.Name = mesh.name ? mesh.name : ("mesh_" + std::to_string(mi));
            results.push_back(std::move(gltfMesh));
        }
    }

    cgltf_free(data);
    LOG_INFO("[glTF] 已加载: %s (%zu 个网格)", filepath.c_str(), results.size());
    return results;
}

} // namespace Engine
