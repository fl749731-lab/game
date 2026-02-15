#include "engine/editor/editor.h"
#include "engine/core/ecs.h"
#include "engine/core/log.h"
#include "engine/renderer/light.h"
#include "engine/physics/physics_world.h"

#include <glad/glad.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace Engine {

bool Editor::s_Enabled = false;

void Editor::Init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // 暗色主题 + 自定义
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding  = 4.0f;
    style.GrabRounding   = 3.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.12f, 0.95f);
    style.Colors[ImGuiCol_Header]   = ImVec4(0.2f, 0.25f, 0.4f, 0.8f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.3f, 0.35f, 0.6f, 0.9f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 450");

    LOG_INFO("[Editor] ImGui 初始化完成");
}

void Editor::Shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    LOG_DEBUG("[Editor] 已清理");
}

void Editor::BeginFrame() {
    if (!s_Enabled) return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Editor::Render(Scene& scene, Entity& selectedEntity) {
    if (!s_Enabled) return;

    DrawSceneHierarchy(scene, selectedEntity);
    DrawInspector(scene.GetWorld(), selectedEntity);
    DrawLightEditor(scene);
    DrawPerformance(scene);
}

void Editor::EndFrame() {
    if (!s_Enabled) return;
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// ── 场景层级面板 ────────────────────────────────────────────

void Editor::DrawSceneHierarchy(Scene& scene, Entity& selectedEntity) {
    ImGui::Begin("Scene Hierarchy");

    auto& world = scene.GetWorld();
    for (auto e : world.GetEntities()) {
        auto* tag = world.GetComponent<TagComponent>(e);
        const char* name = tag ? tag->Name.c_str() : "Unnamed";

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (e == selectedEntity) flags |= ImGuiTreeNodeFlags_Selected;

        bool opened = ImGui::TreeNodeEx((void*)(uintptr_t)e, flags, "%s [%u]", name, (u32)e);
        if (ImGui::IsItemClicked()) selectedEntity = e;
        if (opened) ImGui::TreePop();
    }

    ImGui::Separator();
    ImGui::Text("Entities: %u", scene.GetEntityCount());
    ImGui::End();
}

// ── 组件检视器 ──────────────────────────────────────────────

void Editor::DrawInspector(ECSWorld& world, Entity entity) {
    ImGui::Begin("Inspector");

    if (entity == INVALID_ENTITY) {
        ImGui::TextDisabled("Select an entity in hierarchy");
        ImGui::End();
        return;
    }

    // Tag
    if (auto* tag = world.GetComponent<TagComponent>(entity)) {
        char buf[128];
        strncpy(buf, tag->Name.c_str(), sizeof(buf));
        buf[sizeof(buf)-1] = 0;
        if (ImGui::InputText("Name", buf, sizeof(buf)))
            tag->Name = buf;
    }

    ImGui::Separator();

    // Transform
    if (auto* tr = world.GetComponent<TransformComponent>(entity)) {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            float pos[3] = {tr->X, tr->Y, tr->Z};
            if (ImGui::DragFloat3("Position", pos, 0.1f)) {
                tr->X = pos[0]; tr->Y = pos[1]; tr->Z = pos[2];
            }
            float rot[3] = {tr->RotX, tr->RotY, tr->RotZ};
            if (ImGui::DragFloat3("Rotation", rot, 1.0f)) {
                tr->RotX = rot[0]; tr->RotY = rot[1]; tr->RotZ = rot[2];
            }
            float scl[3] = {tr->ScaleX, tr->ScaleY, tr->ScaleZ};
            if (ImGui::DragFloat3("Scale", scl, 0.1f, 0.01f, 100.0f)) {
                tr->ScaleX = scl[0]; tr->ScaleY = scl[1]; tr->ScaleZ = scl[2];
            }
        }
    }

    // Render
    if (auto* rc = world.GetComponent<RenderComponent>(entity)) {
        if (ImGui::CollapsingHeader("Render")) {
            char meshBuf[64];
            strncpy(meshBuf, rc->MeshType.c_str(), sizeof(meshBuf));
            meshBuf[sizeof(meshBuf)-1] = 0;
            if (ImGui::InputText("Mesh Type", meshBuf, sizeof(meshBuf)))
                rc->MeshType = meshBuf;
            float col[3] = {rc->ColorR, rc->ColorG, rc->ColorB};
            if (ImGui::ColorEdit3("Color", col)) {
                rc->ColorR = col[0]; rc->ColorG = col[1]; rc->ColorB = col[2];
            }
        }
    }

    // Material
    if (auto* mat = world.GetComponent<MaterialComponent>(entity)) {
        if (ImGui::CollapsingHeader("Material")) {
            ImGui::DragFloat("Roughness", &mat->Roughness, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Metallic", &mat->Metallic, 0.01f, 0.0f, 1.0f);
            ImGui::Checkbox("Emissive", &mat->Emissive);
            if (mat->Emissive) {
                float emCol[3] = {mat->EmissiveR, mat->EmissiveG, mat->EmissiveB};
                if (ImGui::ColorEdit3("Emissive Color", emCol)) {
                    mat->EmissiveR = emCol[0]; mat->EmissiveG = emCol[1]; mat->EmissiveB = emCol[2];
                }
                ImGui::DragFloat("Emissive Intensity", &mat->EmissiveIntensity, 0.1f, 0.0f, 50.0f);
            }
        }
    }

    // Health
    if (auto* hp = world.GetComponent<HealthComponent>(entity)) {
        if (ImGui::CollapsingHeader("Health")) {
            ImGui::DragFloat("Current", &hp->Current, 1.0f, 0.0f, hp->Max);
            ImGui::DragFloat("Max", &hp->Max, 1.0f, 1.0f, 10000.0f);
        }
    }

    // Velocity
    if (auto* vel = world.GetComponent<VelocityComponent>(entity)) {
        if (ImGui::CollapsingHeader("Velocity")) {
            float v[3] = {vel->VX, vel->VY, vel->VZ};
            if (ImGui::DragFloat3("Velocity", v, 0.1f)) {
                vel->VX = v[0]; vel->VY = v[1]; vel->VZ = v[2];
            }
        }
    }

    // AI
    if (auto* ai = world.GetComponent<AIComponent>(entity)) {
        if (ImGui::CollapsingHeader("AI")) {
            ImGui::Text("State: %s", ai->State.c_str());
            char modBuf[128];
            strncpy(modBuf, ai->ScriptModule.c_str(), sizeof(modBuf));
            modBuf[sizeof(modBuf)-1] = 0;
            if (ImGui::InputText("Script Module", modBuf, sizeof(modBuf)))
                ai->ScriptModule = modBuf;
        }
    }

    // RigidBody
    if (auto* rb = world.GetComponent<RigidBodyComponent>(entity)) {
        if (ImGui::CollapsingHeader("Rigid Body")) {
            ImGui::DragFloat("Mass", &rb->Mass, 0.1f, 0.01f, 10000.0f);
            ImGui::DragFloat("Restitution", &rb->Restitution, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Friction", &rb->Friction, 0.01f, 0.0f, 1.0f);
            ImGui::Checkbox("Is Static", &rb->IsStatic);
            ImGui::Checkbox("Use Gravity", &rb->UseGravity);
            if (rb->UseGravity) {
                ImGui::DragFloat3("Gravity Override", &rb->GravityOverride.x, 0.1f);
            }
            float vel[3] = {rb->Velocity.x, rb->Velocity.y, rb->Velocity.z};
            ImGui::DragFloat3("RB Velocity", vel, 0.1f);
            rb->Velocity = {vel[0], vel[1], vel[2]};
        }
    }

    // Collider
    if (auto* col = world.GetComponent<ColliderComponent>(entity)) {
        if (ImGui::CollapsingHeader("Collider")) {
            ImGui::Checkbox("Is Trigger", &col->IsTrigger);
            ImGui::DragFloat3("AABB Min", &col->LocalBounds.Min.x, 0.1f);
            ImGui::DragFloat3("AABB Max", &col->LocalBounds.Max.x, 0.1f);
            glm::vec3 size = col->LocalBounds.Max - col->LocalBounds.Min;
            ImGui::Text("Size: %.1f x %.1f x %.1f", size.x, size.y, size.z);
        }
    }

    ImGui::End();
}

// ── 光照编辑器 ──────────────────────────────────────────────

void Editor::DrawLightEditor(Scene& scene) {
    ImGui::Begin("Light Editor");

    // 方向光
    auto& dirLight = scene.GetDirLight();
    if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Direction##dir", &dirLight.Direction.x, 0.01f);
        ImGui::ColorEdit3("Color##dir", &dirLight.Color.x);
        ImGui::DragFloat("Intensity##dir", &dirLight.Intensity, 0.1f, 0.0f, 20.0f);
    }

    // 点光源
    auto& pointLights = scene.GetPointLights();
    if (ImGui::CollapsingHeader("Point Lights")) {
        for (int i = 0; i < (int)pointLights.size(); i++) {
            ImGui::PushID(i);
            char label[32];
            snprintf(label, sizeof(label), "Point %d", i);
            if (ImGui::TreeNode(label)) {
                ImGui::DragFloat3("Position", &pointLights[i].Position.x, 0.1f);
                ImGui::ColorEdit3("Color", &pointLights[i].Color.x);
                ImGui::DragFloat("Intensity", &pointLights[i].Intensity, 0.1f, 0.0f, 100.0f);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    // 聚光灯
    auto& spotLights = scene.GetSpotLights();
    if (ImGui::CollapsingHeader("Spot Lights")) {
        for (int i = 0; i < (int)spotLights.size(); i++) {
            ImGui::PushID(1000 + i);
            char label[32];
            snprintf(label, sizeof(label), "Spot %d", i);
            if (ImGui::TreeNode(label)) {
                ImGui::DragFloat3("Position", &spotLights[i].Position.x, 0.1f);
                ImGui::DragFloat3("Direction", &spotLights[i].Direction.x, 0.01f);
                ImGui::ColorEdit3("Color", &spotLights[i].Color.x);
                ImGui::DragFloat("Intensity", &spotLights[i].Intensity, 0.1f, 0.0f, 100.0f);
                ImGui::DragFloat("Inner Cutoff", &spotLights[i].InnerCutoff, 0.5f, 0.0f, 90.0f);
                ImGui::DragFloat("Outer Cutoff", &spotLights[i].OuterCutoff, 0.5f, 0.0f, 90.0f);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    ImGui::End();
}

// ── 性能监控 ─────────────────────────────────────────────────

void Editor::DrawPerformance(Scene& scene) {
    ImGui::Begin("Performance");

    // FPS / 帧时间
    static float fpsHistory[120] = {};
    static int fpsIdx = 0;
    float fps = ImGui::GetIO().Framerate;
    fpsHistory[fpsIdx] = fps;
    fpsIdx = (fpsIdx + 1) % 120;

    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Frame: %.3f ms", 1000.0f / fps);
    ImGui::PlotLines("FPS", fpsHistory, 120, fpsIdx, nullptr, 0.0f, 120.0f, ImVec2(0, 50));

    ImGui::Separator();
    ImGui::Text("Entities: %u", scene.GetEntityCount());
    ImGui::Text("Point Lights: %u", (u32)scene.GetPointLights().size());
    ImGui::Text("Spot Lights: %u", (u32)scene.GetSpotLights().size());

    ImGui::Separator();
    ImGui::Text("OpenGL: %s", (const char*)glGetString(GL_VERSION));
    ImGui::Text("GPU: %s", (const char*)glGetString(GL_RENDERER));

    ImGui::End();
}

} // namespace Engine
