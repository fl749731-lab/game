#include "engine/renderer/gltf_loader.h"
#include "engine/core/log.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Engine {

// ── 辅助：从 accessor 读取数据 ─────────────────────────────

static const void* AccessorData(const cgltf_accessor* accessor) {
    const cgltf_buffer_view* view = accessor->buffer_view;
    return (const u8*)view->buffer->data + view->offset + accessor->offset;
}

// ── 辅助：构建 Skeleton ────────────────────────────────────

static Ref<Skeleton> BuildSkeleton(const cgltf_skin* skin, const cgltf_data* data) {
    auto skeleton = std::make_shared<Skeleton>();
    if (!skin) return skeleton;

    // 逆绑定矩阵
    std::vector<glm::mat4> invBindMats(skin->joints_count, glm::mat4(1.0f));
    if (skin->inverse_bind_matrices) {
        const f32* src = (const f32*)AccessorData(skin->inverse_bind_matrices);
        for (cgltf_size i = 0; i < skin->joints_count; i++) {
            invBindMats[i] = glm::make_mat4(src + i * 16);
        }
    }

    // 添加骨骼
    for (cgltf_size i = 0; i < skin->joints_count; i++) {
        cgltf_node* joint = skin->joints[i];
        Bone bone;
        bone.Name = joint->name ? joint->name : ("bone_" + std::to_string(i));
        bone.InverseBindMatrix = invBindMats[i];

        // 局部变换
        if (joint->has_matrix) {
            bone.LocalTransform = glm::make_mat4(joint->matrix);
        } else {
            glm::vec3 t = {0, 0, 0};
            glm::quat r = glm::quat(1, 0, 0, 0);
            glm::vec3 s = {1, 1, 1};
            if (joint->has_translation) t = {joint->translation[0], joint->translation[1], joint->translation[2]};
            if (joint->has_rotation)    r = glm::quat(joint->rotation[3], joint->rotation[0], joint->rotation[1], joint->rotation[2]);
            if (joint->has_scale)       s = {joint->scale[0], joint->scale[1], joint->scale[2]};

            glm::mat4 T = glm::translate(glm::mat4(1.0f), t);
            glm::mat4 R = glm::mat4_cast(r);
            glm::mat4 S = glm::scale(glm::mat4(1.0f), s);
            bone.LocalTransform = T * R * S;
        }

        // 父骨骼索引
        bone.ParentIndex = -1;
        if (joint->parent) {
            for (cgltf_size j = 0; j < skin->joints_count; j++) {
                if (skin->joints[j] == joint->parent) {
                    bone.ParentIndex = (i32)j;
                    break;
                }
            }
        }

        skeleton->AddBone(bone);
    }

    return skeleton;
}

// ── 辅助：解析动画 ─────────────────────────────────────────

static std::vector<AnimationClip> ParseAnimations(const cgltf_data* data,
                                                    const cgltf_skin* skin) {
    std::vector<AnimationClip> clips;
    if (!skin) return clips;

    for (cgltf_size ai = 0; ai < data->animations_count; ai++) {
        const cgltf_animation& anim = data->animations[ai];
        AnimationClip clip;
        clip.Name = anim.name ? anim.name : ("anim_" + std::to_string(ai));
        clip.Duration = 0.0f;

        for (cgltf_size ci = 0; ci < anim.channels_count; ci++) {
            const cgltf_animation_channel& ch = anim.channels[ci];
            if (!ch.target_node || !ch.sampler) continue;

            // 找到目标骨骼索引
            i32 boneIdx = -1;
            for (cgltf_size j = 0; j < skin->joints_count; j++) {
                if (skin->joints[j] == ch.target_node) {
                    boneIdx = (i32)j;
                    break;
                }
            }
            if (boneIdx < 0) continue;

            // 找到或创建通道
            AnimationChannel* animCh = nullptr;
            for (auto& existing : clip.Channels) {
                if (existing.BoneIndex == boneIdx) { animCh = &existing; break; }
            }
            if (!animCh) {
                clip.Channels.push_back({});
                animCh = &clip.Channels.back();
                animCh->BoneIndex = boneIdx;
            }

            const cgltf_animation_sampler& sampler = *ch.sampler;
            cgltf_size keyCount = sampler.input->count;
            const f32* times = (const f32*)AccessorData(sampler.input);
            const f32* values = (const f32*)AccessorData(sampler.output);

            // 更新总时长
            for (cgltf_size k = 0; k < keyCount; k++) {
                if (times[k] > clip.Duration) clip.Duration = times[k];
            }

            switch (ch.target_path) {
                case cgltf_animation_path_type_translation:
                    for (cgltf_size k = 0; k < keyCount; k++) {
                        animCh->PositionKeys.push_back({times[k], {values[k*3], values[k*3+1], values[k*3+2]}});
                    }
                    break;
                case cgltf_animation_path_type_rotation:
                    for (cgltf_size k = 0; k < keyCount; k++) {
                        // glTF: [x,y,z,w] → glm::quat(w,x,y,z)
                        animCh->RotationKeys.push_back({times[k],
                            glm::quat(values[k*4+3], values[k*4], values[k*4+1], values[k*4+2])});
                    }
                    break;
                case cgltf_animation_path_type_scale:
                    for (cgltf_size k = 0; k < keyCount; k++) {
                        animCh->ScaleKeys.push_back({times[k], {values[k*3], values[k*3+1], values[k*3+2]}});
                    }
                    break;
                default: break;
            }
        }

        clips.push_back(std::move(clip));
    }

    return clips;
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

    // 解析蒙皮和动画 (使用第一个 skin)
    const cgltf_skin* skin = (data->skins_count > 0) ? &data->skins[0] : nullptr;
    Ref<Skeleton> skeleton = BuildSkeleton(skin, data);
    auto clips = ParseAnimations(data, skin);

    bool hasSkinData = skin && skeleton->GetBoneCount() > 0;
    if (hasSkinData) {
        LOG_INFO("[glTF] 蒙皮数据: %u 骨骼, %zu 动画片段",
                 skeleton->GetBoneCount(), clips.size());
    }

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
            const cgltf_accessor* jointAccessor  = nullptr;
            const cgltf_accessor* weightAccessor = nullptr;

            for (cgltf_size ai = 0; ai < prim.attributes_count; ai++) {
                const auto& attr = prim.attributes[ai];
                if (attr.type == cgltf_attribute_type_position)  posAccessor    = attr.data;
                if (attr.type == cgltf_attribute_type_normal)    normAccessor   = attr.data;
                if (attr.type == cgltf_attribute_type_texcoord)  uvAccessor     = attr.data;
                if (attr.type == cgltf_attribute_type_tangent)   tanAccessor    = attr.data;
                if (attr.type == cgltf_attribute_type_joints)    jointAccessor  = attr.data;
                if (attr.type == cgltf_attribute_type_weights)   weightAccessor = attr.data;
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

            // 蒙皮顶点数据
            std::vector<GltfSkinVertex> skinVerts;
            bool meshHasSkin = hasSkinData && jointAccessor && weightAccessor;
            if (meshHasSkin) {
                skinVerts.resize(vertCount);
                for (cgltf_size v = 0; v < vertCount; v++) {
                    skinVerts[v].Position  = vertices[v].Position;
                    skinVerts[v].Normal    = vertices[v].Normal;
                    skinVerts[v].TexCoord  = vertices[v].TexCoord;
                    skinVerts[v].Tangent   = vertices[v].Tangent;
                    skinVerts[v].Bitangent = vertices[v].Bitangent;
                }

                // JOINTS_0 (u8 or u16)
                for (cgltf_size v = 0; v < vertCount; v++) {
                    u32 joints[4] = {0, 0, 0, 0};
                    for (int j = 0; j < 4; j++) {
                        cgltf_accessor_read_uint(jointAccessor, v * 4 + j, &joints[j], 1);
                    }
                    // cgltf_accessor_read_uint returns per-component, but accessor_read_uint reads element
                    // Use the float read path instead for robustness
                    cgltf_float jf[4];
                    cgltf_accessor_read_float(jointAccessor, v, jf, 4);
                    skinVerts[v].BoneIDs = {(int)jf[0], (int)jf[1], (int)jf[2], (int)jf[3]};

                    cgltf_float wf[4];
                    cgltf_accessor_read_float(weightAccessor, v, wf, 4);
                    skinVerts[v].Weights = {wf[0], wf[1], wf[2], wf[3]};
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
                // 同步到蒙皮顶点
                if (meshHasSkin) {
                    for (cgltf_size v = 0; v < vertCount; v++) {
                        skinVerts[v].Tangent   = vertices[v].Tangent;
                        skinVerts[v].Bitangent = vertices[v].Bitangent;
                    }
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
            gltfMesh.HasSkin = meshHasSkin;
            if (meshHasSkin) {
                gltfMesh.SkinVertices = std::move(skinVerts);
                gltfMesh.SkeletonRef = skeleton;
                gltfMesh.Clips = clips;
            }
            results.push_back(std::move(gltfMesh));
        }
    }

    cgltf_free(data);
    LOG_INFO("[glTF] 已加载: %s (%zu 个网格%s)", filepath.c_str(), results.size(),
             hasSkinData ? ", 含蒙皮" : "");
    return results;
}

} // namespace Engine

