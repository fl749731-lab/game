#include "engine/editor/particle_editor.h"
#include "engine/core/log.h"

#include <imgui.h>
#include <cmath>

namespace Engine {

ParticleEditor::ParticleEditor() {}
ParticleEditor::~ParticleEditor() {}

const std::vector<std::pair<std::string, ParticleEditorConfig>>& ParticleEditor::GetPresets() {
    static std::vector<std::pair<std::string, ParticleEditorConfig>> presets;
    if (presets.empty()) {
        // 火焰
        {
            ParticleEditorConfig c;
            c.EmitRate = 80; c.LifetimeMin = 0.5f; c.LifetimeMax = 1.5f;
            c.SpeedMin = 2; c.SpeedMax = 5;
            c.EmitShape = ParticleEditorConfig::Cone; c.ConeAngle = 15;
            c.StartR = 1; c.StartG = 0.6f; c.StartB = 0.1f;
            c.EndR = 0.5f; c.EndG = 0.1f; c.EndB = 0;
            c.StartSize = 0.3f; c.EndSize = 0.05f;
            c.GravityY = 2.0f; c.Additive = true;
            presets.push_back({"火焰", c});
        }
        // 烟雾
        {
            ParticleEditorConfig c;
            c.EmitRate = 30; c.LifetimeMin = 2; c.LifetimeMax = 5;
            c.SpeedMin = 0.5f; c.SpeedMax = 1.5f;
            c.EmitShape = ParticleEditorConfig::Sphere; c.ShapeRadius = 0.5f;
            c.StartR = 0.5f; c.StartG = 0.5f; c.StartB = 0.5f; c.StartA = 0.6f;
            c.EndR = 0.3f; c.EndG = 0.3f; c.EndB = 0.3f; c.EndA = 0;
            c.StartSize = 0.5f; c.EndSize = 2.0f;
            c.GravityY = 0.5f; c.DragCoeff = 0.3f; c.Additive = false;
            presets.push_back({"烟雾", c});
        }
        // 火花
        {
            ParticleEditorConfig c;
            c.EmitRate = 200; c.LifetimeMin = 0.2f; c.LifetimeMax = 0.8f;
            c.SpeedMin = 5; c.SpeedMax = 15;
            c.EmitShape = ParticleEditorConfig::Point;
            c.Spread = 1.0f;
            c.StartR = 1; c.StartG = 0.9f; c.StartB = 0.6f;
            c.EndR = 1; c.EndG = 0.3f; c.EndB = 0;
            c.StartSize = 0.05f; c.EndSize = 0.01f;
            c.GravityY = -9.8f; c.Additive = true;
            c.Mode = ParticleEditorConfig::Stretched;
            presets.push_back({"火花", c});
        }
        // 雪花
        {
            ParticleEditorConfig c;
            c.EmitRate = 40; c.LifetimeMin = 3; c.LifetimeMax = 8;
            c.SpeedMin = 0.2f; c.SpeedMax = 0.8f;
            c.EmitShape = ParticleEditorConfig::Box; c.ShapeRadius = 10;
            c.DirY = -1; c.Spread = 0.2f;
            c.StartR = 1; c.StartG = 1; c.StartB = 1; c.StartA = 0.8f;
            c.EndA = 0.2f;
            c.StartSize = 0.1f; c.EndSize = 0.15f;
            c.GravityY = -0.5f; c.DragCoeff = 0.5f; c.Additive = false;
            presets.push_back({"雪花", c});
        }
    }
    return presets;
}

void ParticleEditor::Render(const char* title) {
    if (!ImGui::Begin(title)) { ImGui::End(); return; }

    ImGui::BeginChild("ParamRegion", ImVec2(-210, 0), true);
    RenderEmitSection();
    RenderShapeSection();
    RenderSizeColorSection();
    RenderForceSection();
    RenderRenderSection();
    RenderPresetSection();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("PreviewRegion", ImVec2(0, 0), true);
    RenderPreview();
    ImGui::EndChild();

    ImGui::End();
}

void ParticleEditor::RenderEmitSection() {
    if (!ImGui::CollapsingHeader("发射参数", ImGuiTreeNodeFlags_DefaultOpen)) return;
    ImGui::DragFloat("发射率", &m_Config.EmitRate, 1.0f, 0, 1000);
    ImGui::DragFloatRange2("生命周期", &m_Config.LifetimeMin, &m_Config.LifetimeMax, 0.1f, 0.01f, 30);
    ImGui::DragFloatRange2("速度", &m_Config.SpeedMin, &m_Config.SpeedMax, 0.1f, 0, 100);
    ImGui::DragInt("最大粒子数", (int*)&m_Config.MaxParticles, 10, 10, 100000);
}

void ParticleEditor::RenderShapeSection() {
    if (!ImGui::CollapsingHeader("形状")) return;
    const char* shapes[] = {"点 (Point)", "球 (Sphere)", "锥 (Cone)", "盒 (Box)"};
    int shape = (int)m_Config.EmitShape;
    if (ImGui::Combo("发射形状", &shape, shapes, 4))
        m_Config.EmitShape = (ParticleEditorConfig::Shape)shape;

    if (m_Config.EmitShape != ParticleEditorConfig::Point)
        ImGui::DragFloat("半径", &m_Config.ShapeRadius, 0.1f, 0, 100);
    if (m_Config.EmitShape == ParticleEditorConfig::Cone)
        ImGui::DragFloat("锥角", &m_Config.ConeAngle, 1, 0, 90);

    ImGui::DragFloat3("方向", &m_Config.DirX, 0.05f);
    ImGui::DragFloat("散布", &m_Config.Spread, 0.05f, 0, 2);
}

void ParticleEditor::RenderSizeColorSection() {
    if (!ImGui::CollapsingHeader("大小 & 颜色", ImGuiTreeNodeFlags_DefaultOpen)) return;
    ImGui::DragFloat("起始大小", &m_Config.StartSize, 0.01f, 0, 10);
    ImGui::DragFloat("结束大小", &m_Config.EndSize, 0.01f, 0, 10);
    ImGui::Separator();
    ImGui::ColorEdit4("起始颜色", &m_Config.StartR);
    ImGui::ColorEdit4("结束颜色", &m_Config.EndR);
}

void ParticleEditor::RenderForceSection() {
    if (!ImGui::CollapsingHeader("力")) return;
    ImGui::DragFloat("重力 Y", &m_Config.GravityY, 0.1f, -30, 30);
    ImGui::DragFloat("阻力", &m_Config.DragCoeff, 0.01f, 0, 2);
}

void ParticleEditor::RenderRenderSection() {
    if (!ImGui::CollapsingHeader("渲染")) return;
    const char* modes[] = {"广告牌 (Billboard)", "拉伸 (Stretched)", "尾迹 (Trail)"};
    int mode = (int)m_Config.Mode;
    if (ImGui::Combo("渲染模式", &mode, modes, 3))
        m_Config.Mode = (ParticleEditorConfig::RenderMode)mode;

    ImGui::Checkbox("叠加混合", &m_Config.Additive);

    static char texBuf[128];
    strncpy(texBuf, m_Config.TexturePath.c_str(), sizeof(texBuf) - 1);
    texBuf[sizeof(texBuf) - 1] = '\0';
    if (ImGui::InputText("纹理", texBuf, sizeof(texBuf)))
        m_Config.TexturePath = texBuf;
}

void ParticleEditor::RenderPresetSection() {
    if (!ImGui::CollapsingHeader("预设")) return;
    auto& presets = GetPresets();
    for (auto& [name, cfg] : presets) {
        if (ImGui::Button(name.c_str(), ImVec2(-1, 0))) {
            m_Config = cfg;
        }
    }
}

void ParticleEditor::RenderPreview() {
    ImGui::Text("粒子预览");
    ImGui::Separator();

    // 简单模拟预览
    m_PreviewTime += ImGui::GetIO().DeltaTime;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 center = ImGui::GetCursorScreenPos();
    center.x += 100; center.y += 100;

    int count = (int)(m_Config.EmitRate * 0.5f);
    if (count > 200) count = 200;

    for (int i = 0; i < count; i++) {
        f32 t = ((f32)i / count) * m_Config.LifetimeMax;
        f32 lifeT = t / m_Config.LifetimeMax;

        f32 speed = m_Config.SpeedMin + (m_Config.SpeedMax - m_Config.SpeedMin) * 0.5f;
        f32 angle = (f32)i * 2.399f + m_PreviewTime;  // 黄金角
        f32 px = std::cos(angle) * speed * t * 10.0f * m_Config.Spread;
        f32 py = -m_Config.DirY * speed * t * 10.0f + m_Config.GravityY * t * t * 5.0f;

        f32 size = m_Config.StartSize + (m_Config.EndSize - m_Config.StartSize) * lifeT;
        f32 r = m_Config.StartR + (m_Config.EndR - m_Config.StartR) * lifeT;
        f32 g = m_Config.StartG + (m_Config.EndG - m_Config.StartG) * lifeT;
        f32 b = m_Config.StartB + (m_Config.EndB - m_Config.StartB) * lifeT;
        f32 a = m_Config.StartA + (m_Config.EndA - m_Config.StartA) * lifeT;

        dl->AddCircleFilled(ImVec2(center.x + px, center.y + py),
                            size * 15.0f,
                            ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a)));
    }
}

} // namespace Engine
