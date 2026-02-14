#include "engine/editor/material_editor.h"
#include "engine/core/log.h"

#include <imgui.h>

namespace Engine {

MaterialEditor::MaterialEditor() {
    LOG_INFO("[MaterialEditor] 创建");
}

MaterialEditor::~MaterialEditor() {
    LOG_INFO("[MaterialEditor] 销毁");
}

void MaterialEditor::RegisterMaterialNodes() {
    using NodeTemplate = NodeGraphEditor::NodeTemplate;

    // 输出节点
    m_Graph.RegisterTemplate(NodeTemplate{
        "输出 (Output)", NodeCategory::Utility,
        [](NodeGraphEditor& g, ImVec2 pos) -> u32 {
            u32 id = g.AddNode("输出", pos, NodeCategory::Utility, IM_COL32(200, 60, 60, 255));
            g.AddPin(id, "漫反射", PinType::Color, PinDir::Input);
            g.AddPin(id, "高光", PinType::Color, PinDir::Input);
            g.AddPin(id, "法线", PinType::Color, PinDir::Input);
            g.AddPin(id, "粗糙度", PinType::Float, PinDir::Input);
            g.AddPin(id, "金属度", PinType::Float, PinDir::Input);
            g.AddPin(id, "自发光", PinType::Color, PinDir::Input);
            g.AddPin(id, "透明度", PinType::Float, PinDir::Input);
            return id;
        }
    });

    // 颜色节点
    m_Graph.RegisterTemplate(NodeTemplate{
        "颜色 (Color)", NodeCategory::Variables,
        [](NodeGraphEditor& g, ImVec2 pos) -> u32 {
            u32 id = g.AddNode("颜色", pos, NodeCategory::Variables, IM_COL32(180, 120, 60, 255));
            g.AddPin(id, "RGB", PinType::Color, PinDir::Output);
            return id;
        }
    });

    // 纹理节点
    m_Graph.RegisterTemplate(NodeTemplate{
        "纹理 (Texture)", NodeCategory::Texture,
        [](NodeGraphEditor& g, ImVec2 pos) -> u32 {
            u32 id = g.AddNode("纹理采样", pos, NodeCategory::Texture, IM_COL32(60, 140, 200, 255));
            g.AddPin(id, "UV", PinType::Vec2, PinDir::Input);
            g.AddPin(id, "RGB", PinType::Color, PinDir::Output);
            g.AddPin(id, "A", PinType::Float, PinDir::Output);
            return id;
        }
    });

    // 法线贴图
    m_Graph.RegisterTemplate(NodeTemplate{
        "法线 (Normal)", NodeCategory::Texture,
        [](NodeGraphEditor& g, ImVec2 pos) -> u32 {
            u32 id = g.AddNode("法线贴图", pos, NodeCategory::Texture, IM_COL32(120, 120, 200, 255));
            g.AddPin(id, "UV", PinType::Vec2, PinDir::Input);
            g.AddPin(id, "Normal", PinType::Color, PinDir::Output);
            return id;
        }
    });

    // 浮点值
    m_Graph.RegisterTemplate(NodeTemplate{
        "浮点 (Float)", NodeCategory::Variables,
        [](NodeGraphEditor& g, ImVec2 pos) -> u32 {
            u32 id = g.AddNode("浮点", pos, NodeCategory::Variables, IM_COL32(150, 150, 150, 255));
            g.AddPin(id, "值", PinType::Float, PinDir::Output);
            return id;
        }
    });

    // 乘法
    m_Graph.RegisterTemplate(NodeTemplate{
        "乘法 (Multiply)", NodeCategory::Math,
        [](NodeGraphEditor& g, ImVec2 pos) -> u32 {
            u32 id = g.AddNode("乘法", pos, NodeCategory::Math, IM_COL32(100, 180, 100, 255));
            g.AddPin(id, "A", PinType::Float, PinDir::Input);
            g.AddPin(id, "B", PinType::Float, PinDir::Input);
            g.AddPin(id, "结果", PinType::Float, PinDir::Output);
            return id;
        }
    });

    // 加法
    m_Graph.RegisterTemplate(NodeTemplate{
        "加法 (Add)", NodeCategory::Math,
        [](NodeGraphEditor& g, ImVec2 pos) -> u32 {
            u32 id = g.AddNode("加法", pos, NodeCategory::Math, IM_COL32(100, 180, 100, 255));
            g.AddPin(id, "A", PinType::Float, PinDir::Input);
            g.AddPin(id, "B", PinType::Float, PinDir::Input);
            g.AddPin(id, "结果", PinType::Float, PinDir::Output);
            return id;
        }
    });

    // Lerp 线性插值
    m_Graph.RegisterTemplate(NodeTemplate{
        "Lerp", NodeCategory::Math,
        [](NodeGraphEditor& g, ImVec2 pos) -> u32 {
            u32 id = g.AddNode("Lerp", pos, NodeCategory::Math, IM_COL32(100, 150, 200, 255));
            g.AddPin(id, "A", PinType::Float, PinDir::Input);
            g.AddPin(id, "B", PinType::Float, PinDir::Input);
            g.AddPin(id, "Alpha", PinType::Float, PinDir::Input);
            g.AddPin(id, "结果", PinType::Float, PinDir::Output);
            return id;
        }
    });

    // Fresnel 菲涅尔
    m_Graph.RegisterTemplate(NodeTemplate{
        "Fresnel", NodeCategory::Math,
        [](NodeGraphEditor& g, ImVec2 pos) -> u32 {
            u32 id = g.AddNode("Fresnel", pos, NodeCategory::Math, IM_COL32(180, 100, 200, 255));
            g.AddPin(id, "Power", PinType::Float, PinDir::Input);
            g.AddPin(id, "结果", PinType::Float, PinDir::Output);
            return id;
        }
    });

    // UV 坐标
    m_Graph.RegisterTemplate(NodeTemplate{
        "UV 坐标", NodeCategory::Variables,
        [](NodeGraphEditor& g, ImVec2 pos) -> u32 {
            u32 id = g.AddNode("UV", pos, NodeCategory::Variables, IM_COL32(200, 200, 80, 255));
            g.AddPin(id, "UV", PinType::Vec2, PinDir::Output);
            return id;
        }
    });

    // 时间
    m_Graph.RegisterTemplate(NodeTemplate{
        "时间 (Time)", NodeCategory::Variables,
        [](NodeGraphEditor& g, ImVec2 pos) -> u32 {
            u32 id = g.AddNode("时间", pos, NodeCategory::Variables, IM_COL32(200, 80, 200, 255));
            g.AddPin(id, "秒", PinType::Float, PinDir::Output);
            g.AddPin(id, "Sin", PinType::Float, PinDir::Output);
            return id;
        }
    });
}

void MaterialEditor::CreateDefaultLayout() {
    m_Graph.ClearAll();
    RegisterMaterialNodes();

    // 创建默认节点
    u32 outputNode = m_Graph.AddNode("输出", ImVec2(600, 200),
        NodeCategory::Utility, IM_COL32(200, 60, 60, 255));
    m_Graph.AddPin(outputNode, "漫反射", PinType::Color, PinDir::Input);
    m_Graph.AddPin(outputNode, "高光", PinType::Color, PinDir::Input);
    m_Graph.AddPin(outputNode, "法线", PinType::Color, PinDir::Input);
    m_Graph.AddPin(outputNode, "粗糙度", PinType::Float, PinDir::Input);
    m_Graph.AddPin(outputNode, "金属度", PinType::Float, PinDir::Input);

    u32 colorNode = m_Graph.AddNode("颜色", ImVec2(200, 200),
        NodeCategory::Variables, IM_COL32(180, 120, 60, 255));
    m_Graph.AddPin(colorNode, "RGB", PinType::Color, PinDir::Output);

    LOG_INFO("[MaterialEditor] 默认布局已创建");
}

void MaterialEditor::Render(const char* title) {
    if (!ImGui::Begin(title)) { ImGui::End(); return; }

    // 工具栏
    if (ImGui::Button("重置")) CreateDefaultLayout();
    ImGui::SameLine();
    if (ImGui::Button("导出参数")) {
        auto params = ExportParams();
        LOG_INFO("[MaterialEditor] 导出: Diffuse(%.2f,%.2f,%.2f) Rough=%.2f Metal=%.2f",
                 params.DiffuseR, params.DiffuseG, params.DiffuseB,
                 params.Roughness, params.Metallic);
    }

    ImGui::Separator();

    // 节点图
    m_Graph.Render("##MaterialNodeGraph");

    ImGui::End();

    // 预览面板
    RenderPreview();
}

void MaterialEditor::RenderPreview() {
    if (!ImGui::Begin("材质预览##MatPreview")) { ImGui::End(); return; }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float size = avail.x < avail.y ? avail.x : avail.y;
    if (size < 10) size = 100;

    // 渐变色模拟预览
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddRectFilledMultiColor(p, ImVec2(p.x + size, p.y + size),
        IM_COL32(200, 100, 50, 255), IM_COL32(100, 200, 150, 255),
        IM_COL32(50, 100, 200, 255), IM_COL32(150, 50, 100, 255));
    dl->AddRect(p, ImVec2(p.x + size, p.y + size), IM_COL32(200, 200, 200, 255));
    ImGui::Dummy(ImVec2(size, size));

    ImGui::Text("材质球预览 (示意)");

    ImGui::End();
}

MaterialEditor::MaterialParams MaterialEditor::ExportParams() const {
    MaterialParams params;
    // TODO: 从节点图计算最终参数
    // 暂时返回默认值
    return params;
}

} // namespace Engine
