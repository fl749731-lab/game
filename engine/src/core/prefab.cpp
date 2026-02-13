#include "engine/core/prefab.h"
#include "engine/core/log.h"

#include <fstream>
#include <sstream>

namespace Engine {

// ── PrefabManager 静态成员 ───────────────────────────────────

std::unordered_map<std::string, Ref<Prefab>> PrefabManager::s_Prefabs;

void PrefabManager::Register(const std::string& name, Ref<Prefab> prefab) {
    s_Prefabs[name] = prefab;
    LOG_INFO("[PrefabManager] Registered prefab: %s", name.c_str());
}

Ref<Prefab> PrefabManager::Get(const std::string& name) {
    auto it = s_Prefabs.find(name);
    return (it != s_Prefabs.end()) ? it->second : nullptr;
}

bool PrefabManager::Has(const std::string& name) {
    return s_Prefabs.count(name) > 0;
}

void PrefabManager::Clear() { s_Prefabs.clear(); }

void PrefabManager::LoadFromDirectory(const std::string& dirPath) {
    LOG_INFO("[PrefabManager] Loading prefabs from: %s", dirPath.c_str());
    // TODO: 遍历目录加载 .prefab 文件
}

// ── 从实体捕获预制体 ────────────────────────────────────────

Ref<Prefab> Prefab::CaptureFromEntity(ECSWorld& world, Entity e,
                                       const std::string& prefabName) {
    auto prefab = std::make_shared<Prefab>(prefabName);
    CaptureEntity(world, e, prefab->m_Root);
    return prefab;
}

void Prefab::CaptureEntity(ECSWorld& world, Entity e, EntityBlueprint& bp) {
    // 名称
    auto* tag = world.GetComponent<TagComponent>(e);
    bp.Name = tag ? tag->Name : "Entity";

    // TransformComponent
    auto* tr = world.GetComponent<TransformComponent>(e);
    if (tr) {
        ComponentSnapshot snap;
        snap.TypeName = "Transform";
        snap.FloatValues["X"] = tr->X;
        snap.FloatValues["Y"] = tr->Y;
        snap.FloatValues["Z"] = tr->Z;
        snap.FloatValues["RotX"] = tr->RotX;
        snap.FloatValues["RotY"] = tr->RotY;
        snap.FloatValues["RotZ"] = tr->RotZ;
        snap.FloatValues["ScaleX"] = tr->ScaleX;
        snap.FloatValues["ScaleY"] = tr->ScaleY;
        snap.FloatValues["ScaleZ"] = tr->ScaleZ;
        bp.Components.push_back(snap);

        // 递归子实体
        for (Entity child : tr->Children) {
            EntityBlueprint childBp;
            CaptureEntity(world, child, childBp);
            bp.Children.push_back(childBp);
        }
    }

    // RenderComponent
    auto* rc = world.GetComponent<RenderComponent>(e);
    if (rc) {
        ComponentSnapshot snap;
        snap.TypeName = "Render";
        snap.StringValues["MeshType"] = rc->MeshType;
        snap.StringValues["ObjPath"] = rc->ObjPath;
        snap.FloatValues["ColorR"] = rc->ColorR;
        snap.FloatValues["ColorG"] = rc->ColorG;
        snap.FloatValues["ColorB"] = rc->ColorB;
        snap.FloatValues["Shininess"] = rc->Shininess;
        bp.Components.push_back(snap);
    }

    // MaterialComponent
    auto* mat = world.GetComponent<MaterialComponent>(e);
    if (mat) {
        ComponentSnapshot snap;
        snap.TypeName = "Material";
        snap.FloatValues["DiffuseR"] = mat->DiffuseR;
        snap.FloatValues["DiffuseG"] = mat->DiffuseG;
        snap.FloatValues["DiffuseB"] = mat->DiffuseB;
        snap.FloatValues["Roughness"] = mat->Roughness;
        snap.FloatValues["Metallic"] = mat->Metallic;
        snap.FloatValues["Emissive"] = mat->Emissive ? 1.0f : 0.0f;
        snap.FloatValues["EmissiveR"] = mat->EmissiveR;
        snap.FloatValues["EmissiveG"] = mat->EmissiveG;
        snap.FloatValues["EmissiveB"] = mat->EmissiveB;
        snap.FloatValues["EmissiveIntensity"] = mat->EmissiveIntensity;
        snap.StringValues["TextureName"] = mat->TextureName;
        snap.StringValues["NormalMapName"] = mat->NormalMapName;
        bp.Components.push_back(snap);
    }
}

// ── 实例化 ──────────────────────────────────────────────────

Entity Prefab::Instantiate(ECSWorld& world, Entity parent) const {
    return InstantiateBlueprint(world, m_Root, parent);
}

Entity Prefab::InstantiateBlueprint(ECSWorld& world, const EntityBlueprint& bp,
                                     Entity parent) const {
    Entity e = world.CreateEntity(bp.Name);

    // 应用组件
    for (const auto& snap : bp.Components) {
        ApplySnapshot(world, e, snap);
    }

    // 设置父子关系
    if (parent != INVALID_ENTITY) {
        world.SetParent(e, parent);
    }

    // 递归实例化子实体
    for (const auto& childBp : bp.Children) {
        InstantiateBlueprint(world, childBp, e);
    }

    return e;
}

void Prefab::ApplySnapshot(ECSWorld& world, Entity e,
                            const ComponentSnapshot& snap) {
    auto getFloat = [&](const std::string& key, f32 def = 0.0f) -> f32 {
        auto it = snap.FloatValues.find(key);
        return (it != snap.FloatValues.end()) ? it->second : def;
    };
    auto getString = [&](const std::string& key) -> std::string {
        auto it = snap.StringValues.find(key);
        return (it != snap.StringValues.end()) ? it->second : "";
    };

    if (snap.TypeName == "Transform") {
        auto& tr = world.AddComponent<TransformComponent>(e);
        tr.X = getFloat("X"); tr.Y = getFloat("Y"); tr.Z = getFloat("Z");
        tr.RotX = getFloat("RotX"); tr.RotY = getFloat("RotY"); tr.RotZ = getFloat("RotZ");
        tr.ScaleX = getFloat("ScaleX", 1.0f);
        tr.ScaleY = getFloat("ScaleY", 1.0f);
        tr.ScaleZ = getFloat("ScaleZ", 1.0f);
    }
    else if (snap.TypeName == "Render") {
        auto& rc = world.AddComponent<RenderComponent>(e);
        rc.MeshType = getString("MeshType");
        rc.ObjPath = getString("ObjPath");
        rc.ColorR = getFloat("ColorR", 1.0f);
        rc.ColorG = getFloat("ColorG", 1.0f);
        rc.ColorB = getFloat("ColorB", 1.0f);
        rc.Shininess = getFloat("Shininess", 32.0f);
    }
    else if (snap.TypeName == "Material") {
        auto& mat = world.AddComponent<MaterialComponent>(e);
        mat.DiffuseR = getFloat("DiffuseR", 0.8f);
        mat.DiffuseG = getFloat("DiffuseG", 0.8f);
        mat.DiffuseB = getFloat("DiffuseB", 0.8f);
        mat.Roughness = getFloat("Roughness", 0.5f);
        mat.Metallic = getFloat("Metallic");
        mat.Emissive = getFloat("Emissive") > 0.5f;
        mat.EmissiveR = getFloat("EmissiveR", 1.0f);
        mat.EmissiveG = getFloat("EmissiveG", 1.0f);
        mat.EmissiveB = getFloat("EmissiveB", 1.0f);
        mat.EmissiveIntensity = getFloat("EmissiveIntensity", 1.0f);
        mat.TextureName = getString("TextureName");
        mat.NormalMapName = getString("NormalMapName");
    }
}

// ── 简易 JSON 序列化 ────────────────────────────────────────

static void SerializeBlueprint(std::ostringstream& out,
                                const EntityBlueprint& bp,
                                int indent) {
    std::string pad(indent * 2, ' ');
    out << pad << "{\n";
    out << pad << "  \"name\": \"" << bp.Name << "\",\n";
    out << pad << "  \"components\": [\n";

    for (size_t i = 0; i < bp.Components.size(); i++) {
        const auto& snap = bp.Components[i];
        out << pad << "    { \"type\": \"" << snap.TypeName << "\"";
        for (const auto& [k, v] : snap.FloatValues) {
            out << ", \"" << k << "\": " << v;
        }
        for (const auto& [k, v] : snap.StringValues) {
            out << ", \"" << k << "\": \"" << v << "\"";
        }
        out << " }";
        if (i + 1 < bp.Components.size()) out << ",";
        out << "\n";
    }
    out << pad << "  ],\n";
    out << pad << "  \"children\": [\n";
    for (size_t i = 0; i < bp.Children.size(); i++) {
        SerializeBlueprint(out, bp.Children[i], indent + 2);
        if (i + 1 < bp.Children.size()) out << ",";
        out << "\n";
    }
    out << pad << "  ]\n";
    out << pad << "}";
}

std::string Prefab::Serialize() const {
    std::ostringstream out;
    out << "{ \"prefab\": \"" << m_Name << "\", \"root\":\n";
    SerializeBlueprint(out, m_Root, 1);
    out << "\n}\n";
    return out.str();
}

bool Prefab::SaveToFile(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("[Prefab] Failed to save: %s", path.c_str());
        return false;
    }
    file << Serialize();
    LOG_INFO("[Prefab] Saved: %s", path.c_str());
    return true;
}

Ref<Prefab> Prefab::Deserialize(const std::string& /*json*/) {
    // TODO: 实现 JSON 解析反序列化
    LOG_WARN("[Prefab] Deserialize not yet implemented");
    return nullptr;
}

Ref<Prefab> Prefab::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("[Prefab] Failed to load: %s", path.c_str());
        return nullptr;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return Deserialize(ss.str());
}

} // namespace Engine
