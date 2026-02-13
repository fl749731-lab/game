#include "engine/core/resource_manager.h"

#include <fstream>
#include <sstream>

namespace Engine {

std::unordered_map<std::string, Ref<Shader>> ResourceManager::s_Shaders;
std::unordered_map<std::string, Ref<Texture2D>> ResourceManager::s_Textures;
std::unordered_map<std::string, Scope<Mesh>> ResourceManager::s_Meshes;

// ── Shader ──────────────────────────────────────────────────

Ref<Shader> ResourceManager::LoadShader(const std::string& name,
                                         const std::string& vertSrc,
                                         const std::string& fragSrc) {
    auto it = s_Shaders.find(name);
    if (it != s_Shaders.end()) {
        LOG_DEBUG("[资源] Shader '%s' 已缓存", name.c_str());
        return it->second;
    }
    auto shader = std::make_shared<Shader>(vertSrc, fragSrc);
    s_Shaders[name] = shader;
    LOG_INFO("[资源] Shader '%s' 已加载并缓存", name.c_str());
    return shader;
}

Ref<Shader> ResourceManager::LoadShaderFromFile(const std::string& name,
                                                 const std::string& vertPath,
                                                 const std::string& fragPath) {
    auto it = s_Shaders.find(name);
    if (it != s_Shaders.end()) return it->second;

    auto readFile = [](const std::string& path) -> std::string {
        std::ifstream file(path);
        if (!file.is_open()) {
            LOG_ERROR("[资源] 无法打开 Shader 文件: %s", path.c_str());
            return "";
        }
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    };

    std::string vertSrc = readFile(vertPath);
    std::string fragSrc = readFile(fragPath);
    if (vertSrc.empty() || fragSrc.empty()) return nullptr;

    auto shader = std::make_shared<Shader>(vertSrc, fragSrc);
    s_Shaders[name] = shader;
    LOG_INFO("[资源] Shader '%s' 已从文件加载 (vert=%s, frag=%s)",
        name.c_str(), vertPath.c_str(), fragPath.c_str());
    return shader;
}

Ref<Shader> ResourceManager::GetShader(const std::string& name) {
    auto it = s_Shaders.find(name);
    if (it != s_Shaders.end()) return it->second;
    LOG_WARN("[资源] Shader '%s' 未找到", name.c_str());
    return nullptr;
}

// ── Texture ─────────────────────────────────────────────────

Ref<Texture2D> ResourceManager::LoadTexture(const std::string& name,
                                              const std::string& filepath) {
    auto it = s_Textures.find(name);
    if (it != s_Textures.end()) {
        LOG_DEBUG("[资源] Texture '%s' 已缓存", name.c_str());
        return it->second;
    }
    auto tex = std::make_shared<Texture2D>(filepath);
    if (tex->IsValid()) {
        s_Textures[name] = tex;
        LOG_INFO("[资源] Texture '%s' 已加载并缓存", name.c_str());
        return tex;
    } else {
        LOG_ERROR("[资源] Texture '%s' 加载失败: %s", name.c_str(), filepath.c_str());
        return nullptr;
    }
}

Ref<Texture2D> ResourceManager::GetTexture(const std::string& name) {
    auto it = s_Textures.find(name);
    if (it != s_Textures.end()) return it->second;
    LOG_WARN("[资源] Texture '%s' 未找到", name.c_str());
    return nullptr;
}

// ── Mesh ────────────────────────────────────────────────────

void ResourceManager::StoreMesh(const std::string& name, Scope<Mesh> mesh) {
    s_Meshes[name] = std::move(mesh);
    LOG_INFO("[资源] Mesh '%s' 已存储", name.c_str());
}

Mesh* ResourceManager::GetMesh(const std::string& name) {
    auto it = s_Meshes.find(name);
    if (it != s_Meshes.end()) return it->second.get();
    return nullptr;
}

// ── 全局 ────────────────────────────────────────────────────

void ResourceManager::Clear() {
    LOG_INFO("[资源] 清除全部缓存: %zu shaders, %zu textures, %zu meshes",
        s_Shaders.size(), s_Textures.size(), s_Meshes.size());
    s_Shaders.clear();
    s_Textures.clear();
    s_Meshes.clear();
}

void ResourceManager::PrintStats() {
    LOG_INFO("[资源] 统计: Shaders=%zu, Textures=%zu, Meshes=%zu",
        s_Shaders.size(), s_Textures.size(), s_Meshes.size());
}

// ── Model (glTF / OBJ) ────────────────────────────────────

std::vector<std::string> ResourceManager::LoadModel(const std::string& filepath) {
    std::vector<std::string> names;

    // 检测后缀
    std::string ext;
    auto dot = filepath.rfind('.');
    if (dot != std::string::npos) {
        ext = filepath.substr(dot);
        // 转小写
        for (auto& c : ext) c = (char)tolower((unsigned char)c);
    }

    if (ext == ".gltf" || ext == ".glb") {
        // glTF 加载
        auto meshes = GltfLoader::Load(filepath);
        for (auto& gm : meshes) {
            std::string meshName = "gltf_" + gm.Name;
            StoreMesh(meshName, std::move(gm.MeshData));

            // 自动加载纹理
            if (!gm.Material.BaseColorTexPath.empty()) {
                LoadTexture(meshName + "_albedo", gm.Material.BaseColorTexPath);
            }
            if (!gm.Material.NormalTexPath.empty()) {
                LoadTexture(meshName + "_normal", gm.Material.NormalTexPath);
            }
            if (!gm.Material.MetallicRoughnessTexPath.empty()) {
                LoadTexture(meshName + "_mr", gm.Material.MetallicRoughnessTexPath);
            }

            names.push_back(meshName);
        }
        LOG_INFO("[资源] 模型加载完成: %s (%zu 个 mesh)", filepath.c_str(), names.size());
    }
    else if (ext == ".obj") {
        // OBJ 加载
        auto mesh = Mesh::LoadOBJ(filepath);
        if (mesh) {
            // 提取文件名作为 mesh 名
            std::string meshName = filepath;
            auto lastSlash = meshName.find_last_of("/\\");
            if (lastSlash != std::string::npos) meshName = meshName.substr(lastSlash + 1);
            auto dotPos = meshName.rfind('.');
            if (dotPos != std::string::npos) meshName = meshName.substr(0, dotPos);

            StoreMesh(meshName, std::move(mesh));
            names.push_back(meshName);
            LOG_INFO("[资源] OBJ 加载完成: %s", filepath.c_str());
        }
    }
    else {
        LOG_ERROR("[资源] 不支持的模型格式: %s", filepath.c_str());
    }

    return names;
}

} // namespace Engine
