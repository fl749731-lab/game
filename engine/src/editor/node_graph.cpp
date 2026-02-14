#include "engine/editor/node_graph.h"
#include "engine/core/log.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Engine {

// ── Pin 颜色映射 ────────────────────────────────────────────

static const ImU32 s_PinColors[] = {
    IM_COL32(220, 220, 220, 255), // Flow   — 白
    IM_COL32(220,  80,  80, 255), // Bool   — 红
    IM_COL32( 80, 220, 220, 255), // Int    — 青
    IM_COL32( 80, 220,  80, 255), // Float  — 绿
    IM_COL32(140, 180, 255, 255), // Vec2   — 淡蓝
    IM_COL32(255, 220,  60, 255), // Vec3   — 黄
    IM_COL32(255, 150,  60, 255), // Vec4   — 橙
    IM_COL32(255, 100, 200, 255), // Color  — 彩虹
    IM_COL32(180,  80, 255, 255), // Texture— 紫
    IM_COL32(255, 150, 200, 255), // String — 粉
    IM_COL32( 80, 120, 220, 255), // Object — 蓝
    IM_COL32(150, 150, 150, 255), // Any    — 灰
};

static const char* s_PinTypeNames[] = {
    "Flow","Bool","Int","Float","Vec2","Vec3","Vec4","Color","Texture","String","Object","Any"
};

static const char* s_CategoryNames[] = {
    "数学","逻辑","纹理","工具","变量","自定义"
};

NodeGraphEditor::NodeGraphEditor() { m_FlowAnimTime = 0; }
NodeGraphEditor::~NodeGraphEditor() {}

// ── 类型检查 ────────────────────────────────────────────────

ImU32 NodeGraphEditor::GetPinColor(PinType type) {
    u8 idx = (u8)type;
    return (idx < (u8)PinType::Count) ? s_PinColors[idx] : IM_COL32(150,150,150,255);
}

const char* NodeGraphEditor::GetPinTypeName(PinType type) {
    u8 idx = (u8)type;
    return (idx < (u8)PinType::Count) ? s_PinTypeNames[idx] : "Unknown";
}

const char* NodeGraphEditor::GetCategoryName(NodeCategory cat) {
    u8 idx = (u8)cat;
    return (idx < (u8)NodeCategory::Count) ? s_CategoryNames[idx] : "未知";
}

bool NodeGraphEditor::AreTypesCompatible(PinType from, PinType to) {
    if (from == to) return true;
    if (from == PinType::Any || to == PinType::Any) return true;
    // 自动转换规则
    if (from == PinType::Int && to == PinType::Float) return true;
    if (from == PinType::Float && to == PinType::Int) return true;
    if (from == PinType::Float && (to == PinType::Vec2 || to == PinType::Vec3 || to == PinType::Vec4)) return true;
    if (from == PinType::Vec3 && to == PinType::Color) return true;
    if (from == PinType::Color && to == PinType::Vec3) return true;
    if (from == PinType::Vec4 && to == PinType::Color) return true;
    return false;
}

// ── 节点操作 ────────────────────────────────────────────────

u32 NodeGraphEditor::AddNode(const std::string& title, ImVec2 pos,
                              NodeCategory category, ImU32 color) {
    Node n;
    n.ID = m_NextID++;
    n.Title = title;
    n.Pos = pos;
    n.Category = category;
    n.Color = color;
    m_Nodes.push_back(n);
    return n.ID;
}

void NodeGraphEditor::RemoveNode(u32 nodeID) {
    // 删除相关连线
    m_Links.erase(std::remove_if(m_Links.begin(), m_Links.end(), [&](const Link& l) {
        Pin* from = FindPin(l.FromPinID);
        Pin* to = FindPin(l.ToPinID);
        return (from && FindNodeByPin(l.FromPinID)->ID == nodeID) ||
               (to && FindNodeByPin(l.ToPinID)->ID == nodeID);
    }), m_Links.end());

    m_Nodes.erase(std::remove_if(m_Nodes.begin(), m_Nodes.end(),
        [nodeID](const Node& n) { return n.ID == nodeID; }), m_Nodes.end());
}

Node* NodeGraphEditor::FindNode(u32 nodeID) {
    for (auto& n : m_Nodes) if (n.ID == nodeID) return &n;
    return nullptr;
}

u32 NodeGraphEditor::AddPin(u32 nodeID, const std::string& name, PinType type, PinDir dir) {
    Node* node = FindNode(nodeID);
    if (!node) return 0;
    Pin pin;
    pin.ID = m_NextID++;
    pin.Name = name;
    pin.Type = type;
    pin.Dir = dir;
    if (dir == PinDir::Input) node->Inputs.push_back(pin);
    else node->Outputs.push_back(pin);
    return pin.ID;
}

u32 NodeGraphEditor::AddLink(u32 fromPinID, u32 toPinID) {
    // 类型检查
    Pin* from = FindPin(fromPinID);
    Pin* to = FindPin(toPinID);
    bool valid = true;
    if (from && to) valid = AreTypesCompatible(from->Type, to->Type);

    Link link;
    link.ID = m_NextID++;
    link.FromPinID = fromPinID;
    link.ToPinID = toPinID;
    link.Valid = valid;
    m_Links.push_back(link);
    return link.ID;
}

void NodeGraphEditor::RemoveLink(u32 linkID) {
    m_Links.erase(std::remove_if(m_Links.begin(), m_Links.end(),
        [linkID](const Link& l) { return l.ID == linkID; }), m_Links.end());
}

void NodeGraphEditor::ClearAll() {
    m_Nodes.clear(); m_Links.clear(); m_CommentBoxes.clear(); m_NextID = 1;
}

// ── 组框 ────────────────────────────────────────────────────

u32 NodeGraphEditor::AddCommentBox(const std::string& title, ImVec2 pos, ImVec2 size) {
    CommentBox box;
    box.ID = m_NextID++;
    box.Title = title;
    box.Pos = pos;
    box.Size = size;
    m_CommentBoxes.push_back(box);
    return box.ID;
}

void NodeGraphEditor::RemoveCommentBox(u32 id) {
    m_CommentBoxes.erase(std::remove_if(m_CommentBoxes.begin(), m_CommentBoxes.end(),
        [id](const CommentBox& b) { return b.ID == id; }), m_CommentBoxes.end());
}

// ── 选择 ────────────────────────────────────────────────────

void NodeGraphEditor::SelectNode(u32 nodeID, bool addToSelection) {
    if (!addToSelection) DeselectAll();
    Node* n = FindNode(nodeID);
    if (n) n->Selected = true;
}

void NodeGraphEditor::DeselectAll() {
    for (auto& n : m_Nodes) n.Selected = false;
}

void NodeGraphEditor::SelectAll() {
    for (auto& n : m_Nodes) n.Selected = true;
}

std::vector<u32> NodeGraphEditor::GetSelectedNodeIDs() const {
    std::vector<u32> ids;
    for (auto& n : m_Nodes) if (n.Selected) ids.push_back(n.ID);
    return ids;
}

void NodeGraphEditor::RegisterTemplate(const NodeTemplate& tmpl) {
    m_Templates.push_back(tmpl);
}

// ── 辅助 ────────────────────────────────────────────────────

Pin* NodeGraphEditor::FindPin(u32 pinID) {
    for (auto& n : m_Nodes) {
        for (auto& p : n.Inputs) if (p.ID == pinID) return &p;
        for (auto& p : n.Outputs) if (p.ID == pinID) return &p;
    }
    return nullptr;
}

Node* NodeGraphEditor::FindNodeByPin(u32 pinID) {
    for (auto& n : m_Nodes) {
        for (auto& p : n.Inputs) if (p.ID == pinID) return &n;
        for (auto& p : n.Outputs) if (p.ID == pinID) return &n;
    }
    return nullptr;
}

ImVec2 NodeGraphEditor::ToScreen(ImVec2 canvasPos) const {
    return ImVec2(canvasPos.x * m_Zoom + m_CanvasOffset.x,
                  canvasPos.y * m_Zoom + m_CanvasOffset.y);
}

ImVec2 NodeGraphEditor::ToCanvas(ImVec2 screenPos) const {
    return ImVec2((screenPos.x - m_CanvasOffset.x) / m_Zoom,
                  (screenPos.y - m_CanvasOffset.y) / m_Zoom);
}

// ── 主渲染 ──────────────────────────────────────────────────

void NodeGraphEditor::Render(const char* title) {
    if (!ImGui::Begin(title)) { ImGui::End(); return; }

    // 更新流动动画时间
    m_FlowAnimTime += ImGui::GetIO().DeltaTime;
    if (m_FlowAnimTime > 10000.0f) m_FlowAnimTime = 0;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    m_WindowPos = ImGui::GetCursorScreenPos();
    ImVec2 windowSize = ImGui::GetContentRegionAvail();

    // 画布背景
    dl->AddRectFilled(m_WindowPos,
                      ImVec2(m_WindowPos.x + windowSize.x, m_WindowPos.y + windowSize.y),
                      IM_COL32(30, 30, 35, 255));

    // 网格
    float gridStep = 32.0f * m_Zoom;
    for (float x = fmodf(m_CanvasOffset.x - m_WindowPos.x, gridStep); x < windowSize.x; x += gridStep) {
        dl->AddLine(ImVec2(m_WindowPos.x + x, m_WindowPos.y),
                    ImVec2(m_WindowPos.x + x, m_WindowPos.y + windowSize.y),
                    IM_COL32(50, 50, 55, 100));
    }
    for (float y = fmodf(m_CanvasOffset.y - m_WindowPos.y, gridStep); y < windowSize.y; y += gridStep) {
        dl->AddLine(ImVec2(m_WindowPos.x, m_WindowPos.y + y),
                    ImVec2(m_WindowPos.x + windowSize.x, m_WindowPos.y + y),
                    IM_COL32(50, 50, 55, 100));
    }
    // 粗网格 (每 4 格)
    float bigGridStep = gridStep * 4;
    for (float x = fmodf(m_CanvasOffset.x - m_WindowPos.x, bigGridStep); x < windowSize.x; x += bigGridStep) {
        dl->AddLine(ImVec2(m_WindowPos.x + x, m_WindowPos.y),
                    ImVec2(m_WindowPos.x + x, m_WindowPos.y + windowSize.y),
                    IM_COL32(60, 60, 65, 150));
    }
    for (float y = fmodf(m_CanvasOffset.y - m_WindowPos.y, bigGridStep); y < windowSize.y; y += bigGridStep) {
        dl->AddLine(ImVec2(m_WindowPos.x, m_WindowPos.y + y),
                    ImVec2(m_WindowPos.x + windowSize.x, m_WindowPos.y + y),
                    IM_COL32(60, 60, 65, 150));
    }

    // 保存偏移基准
    ImVec2 savedOffset = m_CanvasOffset;
    m_CanvasOffset.x += m_WindowPos.x;
    m_CanvasOffset.y += m_WindowPos.y;

    RenderCommentBoxes(dl);
    RenderLinks(dl);
    RenderNodes(dl);
    RenderPendingLink(dl);
    HandleMarqueeSelect(dl);

    m_CanvasOffset = savedOffset;

    // 小地图
    if (m_ShowMinimap)
        RenderMinimap(dl, m_WindowPos, windowSize);

    HandleInput();
    RenderContextMenu();
    RenderSearchPopup();

    ImGui::End();
}

// ── 节点渲染 ────────────────────────────────────────────────

void NodeGraphEditor::RenderNodes(ImDrawList* dl) {
    float pinRadius = 6.0f * m_Zoom;
    float headerH = 24.0f * m_Zoom;
    float pinSpacing = 22.0f * m_Zoom;

    for (auto& node : m_Nodes) {
        ImVec2 nodePos = ToScreen(node.Pos);
        u32 pinCount = std::max((u32)node.Inputs.size(), (u32)node.Outputs.size());
        float bodyH = (node.Collapsed ? 0 : pinCount * pinSpacing + 10 * m_Zoom);
        float nodeW = node.Size.x * m_Zoom;
        float nodeH = headerH + bodyH;

        if (node.PreviewTextureID > 0 && !node.Collapsed)
            nodeH += 64 * m_Zoom;

        node.Size.y = nodeH / m_Zoom;

        // 选中发光
        if (node.Selected) {
            dl->AddRect(ImVec2(nodePos.x - 3, nodePos.y - 3),
                        ImVec2(nodePos.x + nodeW + 3, nodePos.y + nodeH + 3),
                        IM_COL32(80, 140, 255, 180), 8.0f, 0, 2.5f);
        }

        // 节点背景
        dl->AddRectFilled(nodePos, ImVec2(nodePos.x + nodeW, nodePos.y + nodeH),
                          IM_COL32(45, 45, 50, 230), 6.0f);

        // 标题栏 (渐变)
        ImU32 headerColor = node.Color;
        ImU32 headerDark = IM_COL32(
            (headerColor & 0xFF) * 3/4,
            ((headerColor >> 8) & 0xFF) * 3/4,
            ((headerColor >> 16) & 0xFF) * 3/4, 255);
        dl->AddRectFilled(nodePos, ImVec2(nodePos.x + nodeW, nodePos.y + headerH),
                          headerColor, 6.0f, ImDrawFlags_RoundCornersTop);

        dl->AddText(ImVec2(nodePos.x + 8 * m_Zoom, nodePos.y + 4 * m_Zoom),
                    IM_COL32(240, 240, 240, 255), node.Title.c_str());

        if (node.Collapsed) continue;

        // 输入 Pins
        float pinYStart = nodePos.y + headerH + 8 * m_Zoom;
        for (size_t i = 0; i < node.Inputs.size(); i++) {
            auto& pin = node.Inputs[i];
            float py = pinYStart + i * pinSpacing;
            pin.ScreenPos = ImVec2(nodePos.x, py);

            ImU32 pinColor = GetPinColor(pin.Type);
            dl->AddCircleFilled(pin.ScreenPos, pinRadius, pinColor);
            dl->AddCircle(pin.ScreenPos, pinRadius, IM_COL32(0,0,0,100));

            dl->AddText(ImVec2(pin.ScreenPos.x + pinRadius + 4, py - 7 * m_Zoom),
                        IM_COL32(200, 200, 200, 255), pin.Name.c_str());
        }

        // 输出 Pins
        for (size_t i = 0; i < node.Outputs.size(); i++) {
            auto& pin = node.Outputs[i];
            float py = pinYStart + i * pinSpacing;
            pin.ScreenPos = ImVec2(nodePos.x + nodeW, py);

            ImU32 pinColor = GetPinColor(pin.Type);
            dl->AddCircleFilled(pin.ScreenPos, pinRadius, pinColor);
            dl->AddCircle(pin.ScreenPos, pinRadius, IM_COL32(0,0,0,100));

            ImVec2 textSize = ImGui::CalcTextSize(pin.Name.c_str());
            dl->AddText(ImVec2(pin.ScreenPos.x - pinRadius - 4 - textSize.x, py - 7 * m_Zoom),
                        IM_COL32(200, 200, 200, 255), pin.Name.c_str());
        }

        // 缩略图预览
        if (node.PreviewTextureID > 0) {
            float prevY = pinYStart + pinCount * pinSpacing;
            float prevSize = 56 * m_Zoom;
            ImVec2 prevPos(nodePos.x + 4 * m_Zoom, prevY);
            dl->AddImage((ImTextureID)(intptr_t)node.PreviewTextureID,
                         prevPos, ImVec2(prevPos.x + prevSize, prevPos.y + prevSize));
        }

        // 边框
        dl->AddRect(nodePos, ImVec2(nodePos.x + nodeW, nodePos.y + nodeH),
                    IM_COL32(80, 80, 85, 200), 6.0f);
    }
}

// ── 连线渲染 ────────────────────────────────────────────────

void NodeGraphEditor::RenderLinks(ImDrawList* dl) {
    for (auto& link : m_Links) {
        Pin* from = FindPin(link.FromPinID);
        Pin* to = FindPin(link.ToPinID);
        if (!from || !to) continue;

        ImVec2 p0 = from->ScreenPos;
        ImVec2 p3 = to->ScreenPos;
        float dx = std::abs(p3.x - p0.x) * 0.5f;
        if (dx < 40) dx = 40;
        ImVec2 p1(p0.x + dx, p0.y);
        ImVec2 p2(p3.x - dx, p3.y);

        ImU32 color = link.Valid ? GetPinColor(from->Type) : IM_COL32(255, 50, 50, 200);
        float thickness = link.Valid ? 2.5f : 1.5f;

        if (!link.Valid) {
            // 虚线 (间隔绘制)
            for (float t = 0; t < 1.0f; t += 0.04f) {
                float t2 = t + 0.02f;
                if (t2 > 1.0f) t2 = 1.0f;
                ImVec2 a, b;
                // Bezier 采样
                auto Bezier = [&](float s) -> ImVec2 {
                    float u = 1-s;
                    return ImVec2(u*u*u*p0.x + 3*u*u*s*p1.x + 3*u*s*s*p2.x + s*s*s*p3.x,
                                 u*u*u*p0.y + 3*u*u*s*p1.y + 3*u*s*s*p2.y + s*s*s*p3.y);
                };
                a = Bezier(t); b = Bezier(t2);
                dl->AddLine(a, b, color, thickness);
            }
        } else {
            dl->AddBezierCubic(p0, p1, p2, p3, color, thickness);
            DrawFlowAnimation(dl, p0, p1, p2, p3, color);
        }
    }
}

void NodeGraphEditor::DrawFlowAnimation(ImDrawList* dl, ImVec2 p0, ImVec2 p1,
                                          ImVec2 p2, ImVec2 p3, ImU32 color) {
    // 流动小圆点
    float speed = 0.8f;
    float t = fmodf(m_FlowAnimTime * speed, 1.0f);

    auto Bezier = [&](float s) -> ImVec2 {
        float u = 1-s;
        return ImVec2(u*u*u*p0.x + 3*u*u*s*p1.x + 3*u*s*s*p2.x + s*s*s*p3.x,
                     u*u*u*p0.y + 3*u*u*s*p1.y + 3*u*s*s*p2.y + s*s*s*p3.y);
    };

    // 3 个间隔等距的圆点
    for (int i = 0; i < 3; i++) {
        float ti = fmodf(t + i * 0.33f, 1.0f);
        ImVec2 pos = Bezier(ti);
        float alpha = 1.0f - std::abs(ti - 0.5f) * 1.5f;
        alpha = std::max(alpha, 0.2f);
        ImU32 dotColor = (color & 0x00FFFFFF) | ((u32)(alpha * 200) << 24);
        dl->AddCircleFilled(pos, 3.5f * m_Zoom, dotColor);
    }
}

// ── 拖拽连线 ────────────────────────────────────────────────

void NodeGraphEditor::RenderPendingLink(ImDrawList* dl) {
    if (!m_DraggingLink) return;

    Pin* from = FindPin(m_DragFromPinID);
    if (!from) return;

    ImVec2 p0 = from->ScreenPos;
    ImVec2 p3 = m_DragEndPos;
    float dx = std::abs(p3.x - p0.x) * 0.5f;
    if (dx < 40) dx = 40;

    ImVec2 p1, p2;
    if (from->Dir == PinDir::Output) {
        p1 = ImVec2(p0.x + dx, p0.y); p2 = ImVec2(p3.x - dx, p3.y);
    } else {
        p1 = ImVec2(p0.x - dx, p0.y); p2 = ImVec2(p3.x + dx, p3.y);
    }

    ImU32 color = GetPinColor(from->Type);
    dl->AddBezierCubic(p0, p1, p2, p3, color, 2.0f);
}

// ── 组框 ────────────────────────────────────────────────────

void NodeGraphEditor::RenderCommentBoxes(ImDrawList* dl) {
    for (auto& box : m_CommentBoxes) {
        ImVec2 p0 = ToScreen(box.Pos);
        ImVec2 p1 = ImVec2(p0.x + box.Size.x * m_Zoom, p0.y + box.Size.y * m_Zoom);

        dl->AddRectFilled(p0, p1, box.Color, 4.0f);
        dl->AddRect(p0, p1, IM_COL32(100, 100, 120, 150), 4.0f, 0, 1.5f);

        // 标题 (左上角)
        dl->AddText(ImVec2(p0.x + 6, p0.y + 4),
                    IM_COL32(200, 200, 220, 200), box.Title.c_str());
    }
}

// ── 小地图 ──────────────────────────────────────────────────

void NodeGraphEditor::RenderMinimap(ImDrawList* dl, ImVec2 windowPos, ImVec2 windowSize) {
    float mmW = 150, mmH = 100;
    ImVec2 mmPos(windowPos.x + windowSize.x - mmW - 10,
                 windowPos.y + windowSize.y - mmH - 10);

    dl->AddRectFilled(mmPos, ImVec2(mmPos.x + mmW, mmPos.y + mmH),
                      IM_COL32(20, 20, 25, 200), 4.0f);
    dl->AddRect(mmPos, ImVec2(mmPos.x + mmW, mmPos.y + mmH),
                IM_COL32(80, 80, 100, 200), 4.0f);

    if (m_Nodes.empty()) return;

    // 求画布范围
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (auto& n : m_Nodes) {
        minX = std::min(minX, n.Pos.x); minY = std::min(minY, n.Pos.y);
        maxX = std::max(maxX, n.Pos.x + n.Size.x);
        maxY = std::max(maxY, n.Pos.y + n.Size.y);
    }
    float rangeX = maxX - minX + 200;
    float rangeY = maxY - minY + 200;
    if (rangeX < 1) rangeX = 1;
    if (rangeY < 1) rangeY = 1;

    auto MapToMinimap = [&](ImVec2 canvasPos) -> ImVec2 {
        return ImVec2(mmPos.x + ((canvasPos.x - minX + 100) / rangeX) * mmW,
                      mmPos.y + ((canvasPos.y - minY + 100) / rangeY) * mmH);
    };

    // 绘制节点
    for (auto& n : m_Nodes) {
        ImVec2 p0 = MapToMinimap(n.Pos);
        ImVec2 p1 = MapToMinimap(ImVec2(n.Pos.x + n.Size.x, n.Pos.y + n.Size.y));
        ImU32 color = n.Selected ? IM_COL32(100, 150, 255, 200) : n.Color;
        dl->AddRectFilled(p0, p1, color);
    }

    // 视口范围
    ImVec2 viewMin = ToCanvas(windowPos);
    ImVec2 viewMax = ToCanvas(ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y));
    ImVec2 vp0 = MapToMinimap(viewMin);
    ImVec2 vp1 = MapToMinimap(viewMax);
    dl->AddRect(vp0, vp1, IM_COL32(255, 255, 255, 150), 0, 0, 1.5f);
}

// ── 框选 ────────────────────────────────────────────────────

void NodeGraphEditor::HandleMarqueeSelect(ImDrawList* dl) {
    if (m_MarqueeActive) {
        ImVec2 p0(std::min(m_MarqueeStart.x, m_MarqueeEnd.x),
                  std::min(m_MarqueeStart.y, m_MarqueeEnd.y));
        ImVec2 p1(std::max(m_MarqueeStart.x, m_MarqueeEnd.x),
                  std::max(m_MarqueeStart.y, m_MarqueeEnd.y));

        dl->AddRectFilled(p0, p1, IM_COL32(80, 130, 255, 40));
        dl->AddRect(p0, p1, IM_COL32(80, 130, 255, 150), 0, 0, 1.0f);
    }
}

// ── 右键菜单 ────────────────────────────────────────────────

void NodeGraphEditor::RenderContextMenu() {
    if (m_ShowContextMenu) {
        ImGui::OpenPopup("##NodeContextMenu");
        m_ShowContextMenu = false;
    }

    if (ImGui::BeginPopup("##NodeContextMenu")) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1), "创建节点");
        ImGui::Separator();

        // 搜索框
        ImGui::InputText("##Search", m_SearchBuffer, sizeof(m_SearchBuffer));

        std::string filter(m_SearchBuffer);

        // 按类别分组
        for (u8 cat = 0; cat < (u8)NodeCategory::Count; cat++) {
            bool hasItems = false;
            for (auto& t : m_Templates) {
                if ((u8)t.Category != cat) continue;
                if (!filter.empty() && t.Name.find(filter) == std::string::npos) continue;
                hasItems = true;
                break;
            }

            if (!hasItems) continue;

            if (ImGui::BeginMenu(GetCategoryName((NodeCategory)cat))) {
                for (auto& t : m_Templates) {
                    if ((u8)t.Category != cat) continue;
                    if (!filter.empty() && t.Name.find(filter) == std::string::npos) continue;

                    if (ImGui::MenuItem(t.Name.c_str())) {
                        ImVec2 canvasPos = ToCanvas(m_ContextMenuPos);
                        t.Creator(*this, canvasPos);
                    }
                }
                ImGui::EndMenu();
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("全选")) SelectAll();
        if (ImGui::MenuItem("清除选择")) DeselectAll();

        auto selected = GetSelectedNodeIDs();
        if (!selected.empty()) {
            ImGui::Separator();
            if (ImGui::MenuItem("删除选中节点")) {
                for (u32 id : selected) RemoveNode(id);
            }
        }

        ImGui::EndPopup();
    }
}

void NodeGraphEditor::RenderSearchPopup() {
    // Ctrl+Space 搜索 (未来实现)
}

// ── 输入处理 ────────────────────────────────────────────────

void NodeGraphEditor::HandleInput() {
    ImGuiIO& io = ImGui::GetIO();

    if (!ImGui::IsWindowHovered()) return;

    // 右键菜单
    if (ImGui::IsMouseClicked(1)) {
        m_ShowContextMenu = true;
        m_ContextMenuPos = io.MousePos;
    }

    // 中键拖拽平移
    if (ImGui::IsMouseDragging(2)) {
        m_CanvasOffset.x += io.MouseDelta.x;
        m_CanvasOffset.y += io.MouseDelta.y;
    }

    // 缩放
    if (io.MouseWheel != 0) {
        float zoomDelta = io.MouseWheel * 0.1f;
        float oldZoom = m_Zoom;
        m_Zoom = std::clamp(m_Zoom + zoomDelta, 0.2f, 3.0f);

        // 以鼠标位置为中心缩放
        ImVec2 mouseRelative(io.MousePos.x - m_WindowPos.x - m_CanvasOffset.x,
                             io.MousePos.y - m_WindowPos.y - m_CanvasOffset.y);
        float ratio = m_Zoom / oldZoom;
        m_CanvasOffset.x -= mouseRelative.x * (ratio - 1.0f);
        m_CanvasOffset.y -= mouseRelative.y * (ratio - 1.0f);
    }

    // 框选 (左键 + 空白区域)
    float pinHitRadius = 10.0f;
    if (ImGui::IsMouseClicked(0) && !m_DraggingLink) {
        // 检查是否点击了 Pin
        bool hitPin = false;
        for (auto& node : m_Nodes) {
            for (auto& pin : node.Outputs) {
                ImVec2 diff(io.MousePos.x - pin.ScreenPos.x, io.MousePos.y - pin.ScreenPos.y);
                if (diff.x * diff.x + diff.y * diff.y < pinHitRadius * pinHitRadius) {
                    m_DraggingLink = true;
                    m_DragFromPinID = pin.ID;
                    hitPin = true;
                    break;
                }
            }
            if (hitPin) break;
            for (auto& pin : node.Inputs) {
                ImVec2 diff(io.MousePos.x - pin.ScreenPos.x, io.MousePos.y - pin.ScreenPos.y);
                if (diff.x * diff.x + diff.y * diff.y < pinHitRadius * pinHitRadius) {
                    m_DraggingLink = true;
                    m_DragFromPinID = pin.ID;
                    hitPin = true;
                    break;
                }
            }
            if (hitPin) break;
        }

        // 检查是否点击了节点
        if (!hitPin) {
            bool hitNode = false;
            for (auto& node : m_Nodes) {
                ImVec2 nodePos = ToScreen(node.Pos);
                float nodeW = node.Size.x * m_Zoom;
                float nodeH = node.Size.y * m_Zoom;
                if (io.MousePos.x >= nodePos.x && io.MousePos.x <= nodePos.x + nodeW &&
                    io.MousePos.y >= nodePos.y && io.MousePos.y <= nodePos.y + nodeH) {
                    SelectNode(node.ID, io.KeyCtrl);
                    hitNode = true;
                    break;
                }
            }

            // 空白区域 — 开始框选或取消选择
            if (!hitNode) {
                if (!io.KeyCtrl) DeselectAll();
                m_MarqueeActive = true;
                m_MarqueeStart = io.MousePos;
                m_MarqueeEnd = io.MousePos;
            }
        }
    }

    // 拖拽连线更新
    if (m_DraggingLink) {
        m_DragEndPos = io.MousePos;
        if (ImGui::IsMouseReleased(0)) {
            // 检查是否放在了某个 Pin 上
            for (auto& node : m_Nodes) {
                auto CheckPins = [&](std::vector<Pin>& pins) {
                    for (auto& pin : pins) {
                        ImVec2 diff(io.MousePos.x - pin.ScreenPos.x, io.MousePos.y - pin.ScreenPos.y);
                        if (diff.x * diff.x + diff.y * diff.y < pinHitRadius * pinHitRadius) {
                            Pin* fromPin = FindPin(m_DragFromPinID);
                            if (fromPin && fromPin->Dir != pin.Dir) {
                                if (fromPin->Dir == PinDir::Output)
                                    AddLink(m_DragFromPinID, pin.ID);
                                else
                                    AddLink(pin.ID, m_DragFromPinID);
                            }
                        }
                    }
                };
                CheckPins(node.Inputs);
                CheckPins(node.Outputs);
            }
            m_DraggingLink = false;
        }
    }

    // 拖拽选中节点
    if (ImGui::IsMouseDragging(0) && !m_DraggingLink && !m_MarqueeActive) {
        for (auto& node : m_Nodes) {
            if (node.Selected) {
                node.Pos.x += io.MouseDelta.x / m_Zoom;
                node.Pos.y += io.MouseDelta.y / m_Zoom;
            }
        }
    }

    // 框选更新
    if (m_MarqueeActive) {
        m_MarqueeEnd = io.MousePos;
        if (ImGui::IsMouseReleased(0)) {
            // 选中框内的节点
            ImVec2 mp0(std::min(m_MarqueeStart.x, m_MarqueeEnd.x),
                       std::min(m_MarqueeStart.y, m_MarqueeEnd.y));
            ImVec2 mp1(std::max(m_MarqueeStart.x, m_MarqueeEnd.x),
                       std::max(m_MarqueeStart.y, m_MarqueeEnd.y));

            for (auto& node : m_Nodes) {
                ImVec2 nodePos = ToScreen(node.Pos);
                float nodeW = node.Size.x * m_Zoom;
                float nodeH = node.Size.y * m_Zoom;
                ImVec2 nodeMax(nodePos.x + nodeW, nodePos.y + nodeH);

                // 节点中心是否在框内
                float cx = nodePos.x + nodeW * 0.5f;
                float cy = nodePos.y + nodeH * 0.5f;

                if (cx >= mp0.x && cx <= mp1.x && cy >= mp0.y && cy <= mp1.y) {
                    node.Selected = true;
                }
            }

            m_MarqueeActive = false;
        }
    }

    // Delete 键删除选中
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        DeleteSelected();
    }

    // Ctrl 快捷键
    if (io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_Z)) Undo();
        if (ImGui::IsKeyPressed(ImGuiKey_Y)) Redo();
        if (ImGui::IsKeyPressed(ImGuiKey_C)) CopySelected();
        if (ImGui::IsKeyPressed(ImGuiKey_V)) PasteClipboard(ToCanvas(io.MousePos));
        if (ImGui::IsKeyPressed(ImGuiKey_X)) CutSelected();
        if (ImGui::IsKeyPressed(ImGuiKey_A)) SelectAll();
    }
}

// ── 撤销重做 ────────────────────────────────────────────────

void NodeGraphEditor::PushUndo(UndoCommand::Type type) {
    UndoCommand cmd;
    cmd.CmdType = type;
    cmd.SnapshotNodes = m_Nodes;
    cmd.SnapshotLinks = m_Links;
    m_UndoStack.push_back(cmd);
    if (m_UndoStack.size() > MAX_UNDO)
        m_UndoStack.erase(m_UndoStack.begin());
    m_RedoStack.clear();
}

void NodeGraphEditor::Undo() {
    if (m_UndoStack.empty()) return;
    UndoCommand cmd;
    cmd.CmdType = UndoCommand::MoveNodes;
    cmd.SnapshotNodes = m_Nodes;
    cmd.SnapshotLinks = m_Links;
    m_RedoStack.push_back(cmd);

    auto& last = m_UndoStack.back();
    m_Nodes = last.SnapshotNodes;
    m_Links = last.SnapshotLinks;
    m_UndoStack.pop_back();
}

void NodeGraphEditor::Redo() {
    if (m_RedoStack.empty()) return;
    UndoCommand cmd;
    cmd.CmdType = UndoCommand::MoveNodes;
    cmd.SnapshotNodes = m_Nodes;
    cmd.SnapshotLinks = m_Links;
    m_UndoStack.push_back(cmd);

    auto& last = m_RedoStack.back();
    m_Nodes = last.SnapshotNodes;
    m_Links = last.SnapshotLinks;
    m_RedoStack.pop_back();
}

// ── 复制粘贴 ────────────────────────────────────────────────

void NodeGraphEditor::CopySelected() {
    m_Clipboard.clear();
    for (auto& n : m_Nodes) {
        if (n.Selected) m_Clipboard.push_back(n);
    }
}

void NodeGraphEditor::PasteClipboard(ImVec2 pos) {
    if (m_Clipboard.empty()) return;
    PushUndo(UndoCommand::AddNode);

    // 计算剪贴板质心
    ImVec2 center = {0, 0};
    for (auto& n : m_Clipboard) {
        center.x += n.Pos.x; center.y += n.Pos.y;
    }
    center.x /= (float)m_Clipboard.size();
    center.y /= (float)m_Clipboard.size();

    DeselectAll();
    for (auto& src : m_Clipboard) {
        Node n = src;
        n.ID = m_NextID++;
        n.Pos.x = pos.x + (src.Pos.x - center.x);
        n.Pos.y = pos.y + (src.Pos.y - center.y);
        n.Selected = true;
        // 重新分配 Pin ID
        for (auto& p : n.Inputs) p.ID = m_NextID++;
        for (auto& p : n.Outputs) p.ID = m_NextID++;
        m_Nodes.push_back(n);
    }
}

void NodeGraphEditor::CutSelected() {
    CopySelected();
    DeleteSelected();
}

void NodeGraphEditor::DeleteSelected() {
    auto ids = GetSelectedNodeIDs();
    if (ids.empty()) return;
    PushUndo(UndoCommand::RemoveNode);
    for (u32 id : ids) RemoveNode(id);
}

// ── 对齐工具 ────────────────────────────────────────────────

void NodeGraphEditor::AlignSelectedHorizontal() {
    auto ids = GetSelectedNodeIDs();
    if (ids.size() < 2) return;
    PushUndo(UndoCommand::MoveNodes);

    float avgY = 0;
    for (u32 id : ids) {
        Node* n = FindNode(id);
        if (n) avgY += n->Pos.y;
    }
    avgY /= (float)ids.size();

    for (u32 id : ids) {
        Node* n = FindNode(id);
        if (n) n->Pos.y = avgY;
    }
}

void NodeGraphEditor::AlignSelectedVertical() {
    auto ids = GetSelectedNodeIDs();
    if (ids.size() < 2) return;
    PushUndo(UndoCommand::MoveNodes);

    float avgX = 0;
    for (u32 id : ids) {
        Node* n = FindNode(id);
        if (n) avgX += n->Pos.x;
    }
    avgX /= (float)ids.size();

    for (u32 id : ids) {
        Node* n = FindNode(id);
        if (n) n->Pos.x = avgX;
    }
}

void NodeGraphEditor::DistributeSelectedHorizontal() {
    auto ids = GetSelectedNodeIDs();
    if (ids.size() < 3) return;
    PushUndo(UndoCommand::MoveNodes);

    // 按 X 排序
    std::vector<Node*> nodes;
    for (u32 id : ids) { Node* n = FindNode(id); if (n) nodes.push_back(n); }
    std::sort(nodes.begin(), nodes.end(), [](Node* a, Node* b) { return a->Pos.x < b->Pos.x; });

    float minX = nodes.front()->Pos.x;
    float maxX = nodes.back()->Pos.x;
    float step = (maxX - minX) / ((float)nodes.size() - 1);

    for (size_t i = 0; i < nodes.size(); i++)
        nodes[i]->Pos.x = minX + step * (float)i;
}

void NodeGraphEditor::DistributeSelectedVertical() {
    auto ids = GetSelectedNodeIDs();
    if (ids.size() < 3) return;
    PushUndo(UndoCommand::MoveNodes);

    std::vector<Node*> nodes;
    for (u32 id : ids) { Node* n = FindNode(id); if (n) nodes.push_back(n); }
    std::sort(nodes.begin(), nodes.end(), [](Node* a, Node* b) { return a->Pos.y < b->Pos.y; });

    float minY = nodes.front()->Pos.y;
    float maxY = nodes.back()->Pos.y;
    float step = (maxY - minY) / ((float)nodes.size() - 1);

    for (size_t i = 0; i < nodes.size(); i++)
        nodes[i]->Pos.y = minY + step * (float)i;
}

} // namespace Engine
