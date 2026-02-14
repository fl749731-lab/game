#include "engine/editor/inspector_panel.h"
#include "engine/core/log.h"

#include <imgui.h>
#include <cstring>
#include <cstdio>

namespace Engine {

void InspectorPanel::Init() { LOG_INFO("[InspectorPanel] 初始化"); }
void InspectorPanel::Shutdown() { LOG_INFO("[InspectorPanel] 关闭"); }

// ── Vec3 带颜色的拖拽控件 ───────────────────────────────────

bool InspectorPanel::DrawVec3Control(const char* label, f32& x, f32& y, f32& z,
                                      f32 resetValue, f32 speed) {
    bool changed = false;
    ImGui::PushID(label);

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, 80.0f);
    ImGui::Text("%s", label);
    ImGui::NextColumn();



    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));

    float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
    float itemWidth = (ImGui::CalcItemWidth() - lineHeight * 3 - ImGui::GetStyle().ItemInnerSpacing.x * 2) / 3.0f;

    // X (红色)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.15f, 0.15f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.25f, 0.25f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1));
    if (ImGui::Button("X", ImVec2(lineHeight, lineHeight))) { x = resetValue; changed = true; }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    ImGui::PushItemWidth(itemWidth);
    if (ImGui::DragFloat("##X", &x, speed)) changed = true;
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // Y (绿色)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.6f, 0.15f, 1));
    if (ImGui::Button("Y", ImVec2(lineHeight, lineHeight))) { y = resetValue; changed = true; }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    ImGui::PushItemWidth(itemWidth);
    if (ImGui::DragFloat("##Y", &y, speed)) changed = true;
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // Z (蓝色)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.25f, 0.8f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.35f, 0.9f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.2f, 0.7f, 1));
    if (ImGui::Button("Z", ImVec2(lineHeight, lineHeight))) { z = resetValue; changed = true; }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    ImGui::PushItemWidth(itemWidth);
    if (ImGui::DragFloat("##Z", &z, speed)) changed = true;
    ImGui::PopItemWidth();

    ImGui::PopStyleVar();
    ImGui::Columns(1);
    ImGui::PopID();
    return changed;
}

// ── 各组件段 ────────────────────────────────────────────────

void InspectorPanel::DrawTagSection(TagComponent* tag) {
    if (!tag) return;
    static char nameBuf[128];
    strncpy(nameBuf, tag->Name.c_str(), sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = '\0';

    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText("##TagName", nameBuf, sizeof(nameBuf))) {
        tag->Name = nameBuf;
    }
}

void InspectorPanel::DrawTransformSection(TransformComponent* tc) {
    if (!tc) return;
    if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) return;

    DrawVec3Control("位置", tc->X, tc->Y, tc->Z, 0.0f, 0.1f);
    DrawVec3Control("旋转", tc->RotX, tc->RotY, tc->RotZ, 0.0f, 1.0f);
    DrawVec3Control("缩放", tc->ScaleX, tc->ScaleY, tc->ScaleZ, 1.0f, 0.05f);

    if (tc->Parent != INVALID_ENTITY)
        ImGui::Text("父节点: %u", tc->Parent);
    if (!tc->Children.empty())
        ImGui::Text("子节点: %u 个", (u32)tc->Children.size());
}

void InspectorPanel::DrawRenderSection(RenderComponent* rc) {
    if (!rc) return;
    if (!ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen)) return;

    const char* meshTypes[] = {"cube", "sphere", "plane", "obj"};
    int current = 0;
    for (int i = 0; i < 4; i++) {
        if (rc->MeshType == meshTypes[i]) { current = i; break; }
    }
    if (ImGui::Combo("网格类型", &current, meshTypes, 4)) {
        rc->MeshType = meshTypes[current];
    }

    if (rc->MeshType == "obj") {
        static char pathBuf[256];
        strncpy(pathBuf, rc->ObjPath.c_str(), sizeof(pathBuf) - 1);
        pathBuf[sizeof(pathBuf) - 1] = '\0';
        if (ImGui::InputText("OBJ 路径", pathBuf, sizeof(pathBuf))) {
            rc->ObjPath = pathBuf;
        }
    }

    ImGui::ColorEdit3("颜色", &rc->ColorR);
    ImGui::DragFloat("光泽度", &rc->Shininess, 0.5f, 1.0f, 256.0f);
}

void InspectorPanel::DrawMaterialSection(MaterialComponent* mc) {
    if (!mc) return;
    if (!ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) return;

    ImGui::ColorEdit3("漫反射", &mc->DiffuseR);
    ImGui::ColorEdit3("高光", &mc->SpecularR);
    ImGui::DragFloat("光泽度", &mc->Shininess, 0.5f, 1.0f, 256.0f);
    ImGui::Separator();
    ImGui::Text("PBR 参数");
    ImGui::SliderFloat("粗糙度", &mc->Roughness, 0.0f, 1.0f);
    ImGui::SliderFloat("金属度", &mc->Metallic, 0.0f, 1.0f);
    ImGui::Separator();

    // 纹理名
    static char texBuf[128];
    strncpy(texBuf, mc->TextureName.c_str(), sizeof(texBuf) - 1);
    texBuf[sizeof(texBuf) - 1] = '\0';
    if (ImGui::InputText("纹理", texBuf, sizeof(texBuf)))
        mc->TextureName = texBuf;

    static char normBuf[128];
    strncpy(normBuf, mc->NormalMapName.c_str(), sizeof(normBuf) - 1);
    normBuf[sizeof(normBuf) - 1] = '\0';
    if (ImGui::InputText("法线贴图", normBuf, sizeof(normBuf)))
        mc->NormalMapName = normBuf;

    ImGui::Separator();
    ImGui::Checkbox("自发光", &mc->Emissive);
    if (mc->Emissive) {
        ImGui::ColorEdit3("自发光色", &mc->EmissiveR);
        ImGui::DragFloat("自发光强度", &mc->EmissiveIntensity, 0.1f, 0.0f, 100.0f);
    }
}

void InspectorPanel::DrawHealthSection(HealthComponent* hc) {
    if (!hc) return;
    if (!ImGui::CollapsingHeader("Health")) return;

    ImGui::DragFloat("当前", &hc->Current, 1.0f, 0.0f, hc->Max);
    ImGui::DragFloat("最大", &hc->Max, 1.0f, 1.0f, 10000.0f);

    // 血条
    float ratio = hc->Current / (hc->Max > 0 ? hc->Max : 1.0f);
    ImGui::ProgressBar(ratio, ImVec2(-1, 0),
        (std::to_string((int)hc->Current) + "/" + std::to_string((int)hc->Max)).c_str());
}

void InspectorPanel::DrawVelocitySection(VelocityComponent* vc) {
    if (!vc) return;
    if (!ImGui::CollapsingHeader("Velocity")) return;
    DrawVec3Control("速度", vc->VX, vc->VY, vc->VZ, 0.0f, 0.1f);
}

void InspectorPanel::DrawScriptSection(ScriptComponent* sc) {
    if (!sc) return;
    if (!ImGui::CollapsingHeader("Script")) return;

    static char modBuf[128];
    strncpy(modBuf, sc->ScriptModule.c_str(), sizeof(modBuf) - 1);
    modBuf[sizeof(modBuf) - 1] = '\0';
    if (ImGui::InputText("模块", modBuf, sizeof(modBuf)))
        sc->ScriptModule = modBuf;

    ImGui::Checkbox("启用", &sc->Enabled);
    ImGui::Text("已初始化: %s", sc->Initialized ? "是" : "否");
}

void InspectorPanel::DrawAISection(AIComponent* ai) {
    if (!ai) return;
    if (!ImGui::CollapsingHeader("AI")) return;

    static char modBuf[128];
    strncpy(modBuf, ai->ScriptModule.c_str(), sizeof(modBuf) - 1);
    modBuf[sizeof(modBuf) - 1] = '\0';
    if (ImGui::InputText("AI 模块", modBuf, sizeof(modBuf)))
        ai->ScriptModule = modBuf;

    ImGui::Text("状态: %s", ai->State.c_str());
    ImGui::DragFloat("感知范围", &ai->DetectRange, 0.5f, 0.0f, 100.0f);
    ImGui::DragFloat("攻击范围", &ai->AttackRange, 0.5f, 0.0f, 50.0f);
}

// ── 添加组件菜单 ────────────────────────────────────────────

void InspectorPanel::DrawAddComponentMenu(ECSWorld& world, Entity entity) {
    if (ImGui::Button("添加组件", ImVec2(-1, 0))) {
        ImGui::OpenPopup("##AddComponent");
    }

    if (ImGui::BeginPopup("##AddComponent")) {
        if (!world.HasComponent<TransformComponent>(entity)) {
            if (ImGui::MenuItem("Transform")) world.AddComponent<TransformComponent>(entity);
        }
        if (!world.HasComponent<RenderComponent>(entity)) {
            if (ImGui::MenuItem("Render")) world.AddComponent<RenderComponent>(entity);
        }
        if (!world.HasComponent<MaterialComponent>(entity)) {
            if (ImGui::MenuItem("Material")) world.AddComponent<MaterialComponent>(entity);
        }
        if (!world.HasComponent<HealthComponent>(entity)) {
            if (ImGui::MenuItem("Health")) world.AddComponent<HealthComponent>(entity);
        }
        if (!world.HasComponent<VelocityComponent>(entity)) {
            if (ImGui::MenuItem("Velocity")) world.AddComponent<VelocityComponent>(entity);
        }
        if (!world.HasComponent<ScriptComponent>(entity)) {
            if (ImGui::MenuItem("Script")) world.AddComponent<ScriptComponent>(entity);
        }
        if (!world.HasComponent<AIComponent>(entity)) {
            if (ImGui::MenuItem("AI")) world.AddComponent<AIComponent>(entity);
        }
        ImGui::EndPopup();
    }
}

// ── 主渲染 ──────────────────────────────────────────────────

void InspectorPanel::Render(ECSWorld& world, Entity selectedEntity) {
    if (!ImGui::Begin("属性##Inspector")) { ImGui::End(); return; }

    if (selectedEntity == INVALID_ENTITY) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "未选中任何实体");
        ImGui::End();
        return;
    }

    ImGui::Text("实体 ID: %u", selectedEntity);
    ImGui::Separator();

    // Tag (名称)
    DrawTagSection(world.GetComponent<TagComponent>(selectedEntity));
    ImGui::Separator();

    // 各组件
    DrawTransformSection(world.GetComponent<TransformComponent>(selectedEntity));
    DrawRenderSection(world.GetComponent<RenderComponent>(selectedEntity));
    DrawMaterialSection(world.GetComponent<MaterialComponent>(selectedEntity));
    DrawHealthSection(world.GetComponent<HealthComponent>(selectedEntity));
    DrawVelocitySection(world.GetComponent<VelocityComponent>(selectedEntity));
    DrawScriptSection(world.GetComponent<ScriptComponent>(selectedEntity));
    DrawAISection(world.GetComponent<AIComponent>(selectedEntity));

    ImGui::Separator();
    DrawAddComponentMenu(world, selectedEntity);

    ImGui::End();
}

} // namespace Engine
