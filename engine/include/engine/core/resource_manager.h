#pragma once

#include "engine/core/types.h"
#include "engine/renderer/shader.h"
#include "engine/renderer/texture.h"
#include "engine/renderer/mesh.h"
#include "engine/renderer/gltf_loader.h"
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
    /// 直接缓存纹理对象（供 AsyncLoader 使用）
    static void CacheTexture(const std::string& name, Ref<Texture2D> tex);

    // ── 异步加载 ─────────────────────────────────────────────
    static void LoadTextureAsync(const std::string& name,
                                  const std::string& filepath,
                                  std::function<void(Ref<Texture2D>)> callback = nullptr);
    static void LoadModelAsync(const std::string& filepath,
                                std::function<void(std::vector<std::string>)> callback = nullptr);

    // ── Mesh ────────────────────────────────────────────────
    static void StoreMesh(const std::string& name, Scope<Mesh> mesh);
    static Mesh* GetMesh(const std::string& name);

    // ── 全局 ────────────────────────────────────────────────
    static void Clear();
    static void PrintStats();

    // ── Model (glTF / OBJ) ──────────────────────────────────
    /// 根据后缀自动选择加载器，返回模型名称列表
    static std::vector<std::string> LoadModel(const std::string& filepath);

private:
    static std::unordered_map<std::string, Ref<Shader>> s_Shaders;
    static std::unordered_map<std::string, Ref<Texture2D>> s_Textures;
    static std::unordered_map<std::string, Scope<Mesh>> s_Meshes;
};

} // namespace Engine
