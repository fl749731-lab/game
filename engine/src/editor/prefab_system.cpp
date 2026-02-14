#include "engine/editor/prefab_system.h"
#include "engine/core/log.h"

#include <imgui.h>
#include <fstream>
#include <sstream>
#include <cstdio>

namespace Engine {

void PrefabSystem::Init() {
    s_Library.clear();
    s_NextPrefabID = 1;
    LOG_INFO("[Prefab] 系统初始化");
}

void PrefabSystem::Shutdown() {
    s_Library.clear();
    LOG_INFO("[Prefab] 系统关闭");
}

PrefabTemplate PrefabSystem::CreateFromEntity(ECSWorld& world, Entity entity) {
    PrefabTemplate prefab;
    prefab.ID = s_NextPrefabID++;

    auto* tag = world.GetComponent<TagComponent>(entity);
    prefab.Name = tag ? tag->Name : "Prefab";

    // Transform
    auto* tc = world.GetComponent<TransformComponent>(entity);
    if (tc) {
        PrefabComponentData cd;
        cd.TypeName = "TransformComponent";
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f", tc->X); cd.Properties["X"] = buf;
        snprintf(buf, sizeof(buf), "%.3f", tc->Y); cd.Properties["Y"] = buf;
        snprintf(buf, sizeof(buf), "%.3f", tc->Z); cd.Properties["Z"] = buf;
        snprintf(buf, sizeof(buf), "%.3f", tc->RotX); cd.Properties["RotX"] = buf;
        snprintf(buf, sizeof(buf), "%.3f", tc->RotY); cd.Properties["RotY"] = buf;
        snprintf(buf, sizeof(buf), "%.3f", tc->RotZ); cd.Properties["RotZ"] = buf;
        snprintf(buf, sizeof(buf), "%.3f", tc->ScaleX); cd.Properties["ScaleX"] = buf;
        snprintf(buf, sizeof(buf), "%.3f", tc->ScaleY); cd.Properties["ScaleY"] = buf;
        snprintf(buf, sizeof(buf), "%.3f", tc->ScaleZ); cd.Properties["ScaleZ"] = buf;
        prefab.Components.push_back(cd);
    }

    // Render
    auto* rc = world.GetComponent<RenderComponent>(entity);
    if (rc) {
        PrefabComponentData cd;
        cd.TypeName = "RenderComponent";
        cd.Properties["MeshType"] = rc->MeshType;
        cd.Properties["ObjPath"] = rc->ObjPath;
        prefab.Components.push_back(cd);
    }

    // Material
    auto* mc = world.GetComponent<MaterialComponent>(entity);
    if (mc) {
        PrefabComponentData cd;
        cd.TypeName = "MaterialComponent";
        char buf[64];
        snprintf(buf, sizeof(buf), "%.3f", mc->DiffuseR); cd.Properties["DiffuseR"] = buf;
        snprintf(buf, sizeof(buf), "%.3f", mc->DiffuseG); cd.Properties["DiffuseG"] = buf;
        snprintf(buf, sizeof(buf), "%.3f", mc->DiffuseB); cd.Properties["DiffuseB"] = buf;
        snprintf(buf, sizeof(buf), "%.3f", mc->Roughness); cd.Properties["Roughness"] = buf;
        snprintf(buf, sizeof(buf), "%.3f", mc->Metallic);  cd.Properties["Metallic"] = buf;
        cd.Properties["TextureName"] = mc->TextureName;
        prefab.Components.push_back(cd);
    }

    LOG_INFO("[Prefab] 从实体 %u 创建预制件: %s", entity, prefab.Name.c_str());
    return prefab;
}

Entity PrefabSystem::Instantiate(ECSWorld& world, const PrefabTemplate& prefab,
                                   const glm::vec3& position) {
    Entity e = world.CreateEntity(prefab.Name);

    for (auto& cd : prefab.Components) {
        if (cd.TypeName == "TransformComponent") {
            auto& tc = world.AddComponent<TransformComponent>(e);
            auto get = [&](const char* key, f32 def) -> f32 {
                auto it = cd.Properties.find(key);
                return (it != cd.Properties.end()) ? std::stof(it->second) : def;
            };
            tc.X = get("X", 0) + position.x;
            tc.Y = get("Y", 0) + position.y;
            tc.Z = get("Z", 0) + position.z;
            tc.RotX = get("RotX", 0); tc.RotY = get("RotY", 0); tc.RotZ = get("RotZ", 0);
            tc.ScaleX = get("ScaleX", 1); tc.ScaleY = get("ScaleY", 1); tc.ScaleZ = get("ScaleZ", 1);
        }
        else if (cd.TypeName == "RenderComponent") {
            auto& rc = world.AddComponent<RenderComponent>(e);
            auto it = cd.Properties.find("MeshType");
            if (it != cd.Properties.end()) rc.MeshType = it->second;
            it = cd.Properties.find("ObjPath");
            if (it != cd.Properties.end()) rc.ObjPath = it->second;
        }
        else if (cd.TypeName == "MaterialComponent") {
            auto& mc = world.AddComponent<MaterialComponent>(e);
            auto get = [&](const char* key, f32 def) -> f32 {
                auto it = cd.Properties.find(key);
                return (it != cd.Properties.end()) ? std::stof(it->second) : def;
            };
            mc.DiffuseR = get("DiffuseR", 0.8f);
            mc.DiffuseG = get("DiffuseG", 0.8f);
            mc.DiffuseB = get("DiffuseB", 0.8f);
            mc.Roughness = get("Roughness", 0.5f);
            mc.Metallic = get("Metallic", 0.0f);
            auto it = cd.Properties.find("TextureName");
            if (it != cd.Properties.end()) mc.TextureName = it->second;
        }
    }

    // 递归实例化子预制件
    for (auto& child : prefab.Children) {
        Entity childEntity = Instantiate(world, child, position);
        world.SetParent(childEntity, e);
    }

    LOG_INFO("[Prefab] 实例化: %s → Entity %u", prefab.Name.c_str(), e);
    return e;
}

bool PrefabSystem::SavePrefab(const PrefabTemplate& prefab, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "PREFAB " << prefab.Name << "\n";
    for (auto& cd : prefab.Components) {
        file << "COMPONENT " << cd.TypeName << "\n";
        for (auto& [key, val] : cd.Properties) {
            file << "  " << key << "=" << val << "\n";
        }
    }

    LOG_INFO("[Prefab] 已保存: %s", path.c_str());
    return true;
}

PrefabTemplate PrefabSystem::LoadPrefab(const std::string& path) {
    PrefabTemplate prefab;
    std::ifstream file(path);
    if (!file.is_open()) return prefab;

    std::string line;
    PrefabComponentData* currentComp = nullptr;

    while (std::getline(file, line)) {
        if (line.substr(0, 7) == "PREFAB ") {
            prefab.Name = line.substr(7);
            prefab.FilePath = path;
        }
        else if (line.substr(0, 10) == "COMPONENT ") {
            prefab.Components.push_back({});
            currentComp = &prefab.Components.back();
            currentComp->TypeName = line.substr(10);
        }
        else if (currentComp && line.size() > 2 && line[0] == ' ') {
            auto eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string key = line.substr(2, eqPos - 2);
                std::string val = line.substr(eqPos + 1);
                currentComp->Properties[key] = val;
            }
        }
    }

    LOG_INFO("[Prefab] 已加载: %s", path.c_str());
    return prefab;
}

void PrefabSystem::Register(const PrefabTemplate& prefab) {
    s_Library.push_back(prefab);
    LOG_INFO("[Prefab] 注册: %s (ID=%u)", prefab.Name.c_str(), prefab.ID);
}

void PrefabSystem::RenderLibraryPanel(ECSWorld& world) {
    if (!ImGui::Begin("预制件库##PrefabLib")) { ImGui::End(); return; }

    ImGui::Text("预制件: %u 个", (u32)s_Library.size());
    ImGui::Separator();

    for (size_t i = 0; i < s_Library.size(); i++) {
        auto& p = s_Library[i];
        ImGui::PushID((int)i);

        if (ImGui::Selectable(p.Name.c_str())) {
            // 选中
        }

        // 双击实例化
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            Instantiate(world, p, {0, 0, 0});
        }

        // 右键菜单
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("实例化")) {
                Instantiate(world, p, {0, 0, 0});
            }
            if (ImGui::MenuItem("保存到文件")) {
                SavePrefab(p, "prefabs/" + p.Name + ".prefab");
            }
            if (ImGui::MenuItem("删除")) {
                s_Library.erase(s_Library.begin() + i);
                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    ImGui::Separator();
    if (ImGui::Button("加载 .prefab", ImVec2(-1, 0))) {
        // TODO: 文件选择对话框
    }

    ImGui::End();
}

} // namespace Engine
