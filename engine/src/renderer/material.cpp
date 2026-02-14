#include "engine/renderer/material.h"
#include "engine/core/log.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace Engine {

Material::Material(Ref<Shader> shader)
    : m_Shader(shader)
{}

void Material::SetShader(Ref<Shader> shader) {
    m_Shader = shader;
}

void Material::SetTexture(TextureSlot slot, Ref<Texture2D> tex) {
    m_Textures[(u8)slot] = tex;
}

Ref<Texture2D> Material::GetTexture(TextureSlot slot) const {
    return m_Textures[(u8)slot];
}

bool Material::HasTexture(TextureSlot slot) const {
    return m_Textures[(u8)slot] != nullptr;
}

void Material::Bind() const {
    if (!m_Shader) return;
    m_Shader->Bind();

    // 设置 PBR 属性 uniform
    m_Shader->SetVec3("uMaterial.albedo", Props.Albedo.r, Props.Albedo.g, Props.Albedo.b);
    m_Shader->SetFloat("uMaterial.metallic", Props.Metallic);
    m_Shader->SetFloat("uMaterial.roughness", Props.Roughness);
    m_Shader->SetFloat("uMaterial.ao", Props.AO);
    m_Shader->SetVec3("uMaterial.emissive", Props.Emissive.r, Props.Emissive.g, Props.Emissive.b);
    m_Shader->SetFloat("uMaterial.emissiveIntensity", Props.EmissiveIntensity);
    m_Shader->SetFloat("uMaterial.shininess", Props.Shininess);

    // 纹理标记
    m_Shader->SetInt("uMaterial.hasAlbedoMap", HasTexture(TextureSlot::Albedo) ? 1 : 0);
    m_Shader->SetInt("uMaterial.hasNormalMap", HasTexture(TextureSlot::Normal) ? 1 : 0);
    m_Shader->SetInt("uMaterial.hasMetallicRoughnessMap", HasTexture(TextureSlot::MetallicRoughness) ? 1 : 0);

    // 绑定纹理到对应纹理单元
    for (u8 i = 0; i < (u8)TextureSlot::Count; i++) {
        if (m_Textures[i]) {
            glActiveTexture(GL_TEXTURE0 + 3 + i);  // 偏移3 (0-2 给 G-Buffer)
            m_Textures[i]->Bind();
            // 设置采样器 uniform
            const char* names[] = {
                "uAlbedoMap", "uNormalMap", "uMetallicRoughnessMap", "uAOMap", "uEmissiveMap"
            };
            m_Shader->SetInt(names[i], 3 + i);
        }
    }
}

void Material::Unbind() const {
    if (m_Shader) m_Shader->Unbind();
}

u64 Material::GetSortKey() const {
    // 高32位 = Shader ID, 低32位 = 首个纹理 ID
    u64 shaderID = m_Shader ? (u64)m_Shader->GetID() : 0;
    u64 texID = m_Textures[0] ? (u64)m_Textures[0]->GetID() : 0;
    return (shaderID << 32) | (texID & 0xFFFFFFFF);
}

} // namespace Engine
