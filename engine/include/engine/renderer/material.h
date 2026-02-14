#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/texture.h"

#include <glm/glm.hpp>
#include <string>

namespace Engine {

// ── 材质属性 (PBR 参数) ──────────────────────────────────────

struct MaterialProperties {
    glm::vec3 Albedo    = {0.8f, 0.8f, 0.8f};
    f32 Metallic        = 0.0f;
    f32 Roughness       = 0.5f;
    f32 AO              = 1.0f;
    glm::vec3 Emissive  = {0, 0, 0};
    f32 EmissiveIntensity = 0.0f;
    f32 Shininess       = 32.0f;    // 兼容旧 Blinn-Phong
};

// ── 纹理插槽 ────────────────────────────────────────────────

enum class TextureSlot : u8 {
    Albedo     = 0,
    Normal     = 1,
    MetallicRoughness = 2,
    AO         = 3,
    Emissive   = 4,
    Count      = 5
};

// ── Material ────────────────────────────────────────────────
// 统一材质系统：Shader + 属性 + 纹理 → 一个可绑定的渲染单元
// 支持排序键用于渲染排序 (减少 state change)

class Material {
public:
    Material() = default;
    explicit Material(Ref<Shader> shader);

    /// 绑定 Shader + 设置 Uniform + 绑定纹理
    void Bind() const;
    void Unbind() const;

    // ── Shader ──────────────────────────────────────────────
    void SetShader(Ref<Shader> shader);
    Ref<Shader> GetShader() const { return m_Shader; }

    // ── 纹理 ───────────────────────────────────────────────
    void SetTexture(TextureSlot slot, Ref<Texture2D> tex);
    Ref<Texture2D> GetTexture(TextureSlot slot) const;
    bool HasTexture(TextureSlot slot) const;

    // ── 排序键 (用于减少 GPU state change) ──────────────────
    u64 GetSortKey() const;

    // ── 名称 (ResourceManager 缓存用) ──────────────────────
    std::string Name;

    /// PBR 属性
    MaterialProperties Props;

private:
    Ref<Shader> m_Shader;
    Ref<Texture2D> m_Textures[(u8)TextureSlot::Count] = {};
};

} // namespace Engine
