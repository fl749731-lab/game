#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/texture.h"
#include "engine/renderer/mesh.h"
#include "engine/core/log.h"

#include <string>
#include <unordered_map>

namespace Engine {

// ── 资源管理器 ──────────────────────────────────────────────
// 全局单例，统一管理 Shader / Texture / Mesh 的加载、缓存和释放

class ResourceManager {
public:
    // ── Shader ──────────────────────────────────────────────
    static Ref<Shader> LoadShader(const std::string& name,
                                   const std::string& vertSrc,
                                   const std::string& fragSrc);
    static Ref<Shader> LoadShaderFromFile(const std::string& name,
                                          const std::string& vertPath,
                                          const std::string& fragPath);
    static Ref<Shader> GetShader(const std::string& name);

    // ── Texture ─────────────────────────────────────────────
    static Ref<Texture2D> LoadTexture(const std::string& name,
                                       const std::string& filepath);
    static Ref<Texture2D> GetTexture(const std::string& name);

    // ── Mesh ────────────────────────────────────────────────
    static void StoreMesh(const std::string& name, Scope<Mesh> mesh);
    static Mesh* GetMesh(const std::string& name);

    // ── 全局 ────────────────────────────────────────────────
    static void Clear();
    static void PrintStats();

private:
    static std::unordered_map<std::string, Ref<Shader>> s_Shaders;
    static std::unordered_map<std::string, Ref<Texture2D>> s_Textures;
    static std::unordered_map<std::string, Scope<Mesh>> s_Meshes;
};

} // namespace Engine
