#pragma once

#include "engine/core/types.h"

#include <imgui.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Engine {

// ── Pin 类型 ────────────────────────────────────────────────

enum class PinType : u8 {
    Flow = 0,      // 执行流 (白色)
    Bool,          // 布尔 (红色)
    Int,           // 整数 (青色)
    Float,         // 浮点 (绿色)
    Vec2,          // 二维向量 (淡蓝)
    Vec3,          // 三维向量 (黄色)
    Vec4,          // 四维向量 (橙色)
    Color,         // 颜色 (彩虹)
    Texture,       // 纹理 (紫色)
    String,        // 字符串 (粉色)
    Object,        // 通用对象 (蓝色)
    Any,           // 万能类型 (灰色)

    Count
};

// ── Pin 方向 ────────────────────────────────────────────────

enum class PinDir : u8 { Input, Output };

// ── 节点类别 ────────────────────────────────────────────────

enum class NodeCategory : u8 {
    Math = 0,
    Logic,
    Texture,
    Utility,
    Variables,
    Custom,
    Count
};

// ── Pin (端口) ──────────────────────────────────────────────

struct Pin {
    u32 ID = 0;
    std::string Name;
    PinType Type = PinType::Float;
    PinDir Dir = PinDir::Input;

    ImVec2 ScreenPos = {};    // 屏幕坐标 (渲染时计算)
};

// ── Link (连线) ─────────────────────────────────────────────

struct Link {
    u32 ID = 0;
    u32 FromPinID = 0;
    u32 ToPinID = 0;
    bool Valid = true;        // 类型兼容
};

// ── 注释组框 ────────────────────────────────────────────────

struct CommentBox {
    u32 ID = 0;
    std::string Title;
    ImVec2 Pos, Size;
    ImU32 Color = IM_COL32(60, 60, 80, 100);
};

// ── Node (节点) ─────────────────────────────────────────────

struct Node {
    u32 ID = 0;
    std::string Title;
    NodeCategory Category = NodeCategory::Utility;

    ImVec2 Pos = {};
    ImVec2 Size = {180, 0};  // 0 = 自动计算高度
    ImU32 Color = IM_COL32(60, 60, 80, 255);
    bool Selected = false;
    bool Collapsed = false;

    std::vector<Pin> Inputs;
    std::vector<Pin> Outputs;

    // 缩略图预览 (Color/Texture 节点)
    u32 PreviewTextureID = 0;
};

// ── 节点图编辑器 (UE Blueprint 级) ──────────────────────────
//
// 增强特性:
//   1. Pin 颜色按类型区分 + 类型安全连接
//   2. 右键上下文菜单 (创建/删除/复制)
//   3. 节点分类搜索
//   4. 组框 (Comment Box) + 标签
//   5. 小地图 (Minimap)
//   6. 框选 (Marquee Select)
//   7. 数据流动画 (连线上移动小圆点)
//   8. 选中高亮 (蓝色发光边框)
//   9. 拖拽连线时类型检查反馈

class NodeGraphEditor {
public:
    NodeGraphEditor();
    ~NodeGraphEditor();

    // ── 节点操作 ──────────────────────────────────
    u32 AddNode(const std::string& title, ImVec2 pos,
                NodeCategory category = NodeCategory::Utility,
                ImU32 color = IM_COL32(60, 60, 80, 255));
    void RemoveNode(u32 nodeID);
    Node* FindNode(u32 nodeID);

    u32 AddPin(u32 nodeID, const std::string& name, PinType type, PinDir dir);
    u32 AddLink(u32 fromPinID, u32 toPinID);
    void RemoveLink(u32 linkID);
    void ClearAll();

    // ── 组框 ──────────────────────────────────────
    u32 AddCommentBox(const std::string& title, ImVec2 pos, ImVec2 size);
    void RemoveCommentBox(u32 id);

    // ── 选择 ──────────────────────────────────────
    void SelectNode(u32 nodeID, bool addToSelection = false);
    void DeselectAll();
    void SelectAll();
    std::vector<u32> GetSelectedNodeIDs() const;

    // ── 渲染 ──────────────────────────────────────
    void Render(const char* title);

    // ── 节点模板注册 (右键创建菜单) ──────────────
    struct NodeTemplate {
        std::string Name;
        NodeCategory Category;
        std::function<u32(NodeGraphEditor&, ImVec2)> Creator;
    };
    void RegisterTemplate(const NodeTemplate& tmpl);

    // ── 类型兼容性 ──────────────────────────────
    static bool AreTypesCompatible(PinType from, PinType to);
    static ImU32 GetPinColor(PinType type);
    static const char* GetPinTypeName(PinType type);
    static const char* GetCategoryName(NodeCategory cat);

private:
    void RenderNodes(ImDrawList* dl);
    void RenderLinks(ImDrawList* dl);
    void RenderPendingLink(ImDrawList* dl);
    void RenderCommentBoxes(ImDrawList* dl);
    void RenderMinimap(ImDrawList* dl, ImVec2 windowPos, ImVec2 windowSize);
    void RenderContextMenu();
    void RenderSearchPopup();
    void HandleInput();
    void HandleMarqueeSelect(ImDrawList* dl);

    // 连线上的流动动画
    void DrawFlowAnimation(ImDrawList* dl, ImVec2 p0, ImVec2 p1,
                            ImVec2 p2, ImVec2 p3, ImU32 color);

    // 辅助
    Pin* FindPin(u32 pinID);
    Node* FindNodeByPin(u32 pinID);
    ImVec2 ToScreen(ImVec2 canvasPos) const;
    ImVec2 ToCanvas(ImVec2 screenPos) const;

    // ── 数据 ────────────────────────────────────
    std::vector<Node> m_Nodes;
    std::vector<Link> m_Links;
    std::vector<CommentBox> m_CommentBoxes;
    std::vector<NodeTemplate> m_Templates;

    u32 m_NextID = 1;

    // 画布状态
    ImVec2 m_CanvasOffset = {};
    float m_Zoom = 1.0f;
    ImVec2 m_WindowPos = {};    // 窗口屏幕位置

    // 交互状态
    bool m_ShowContextMenu = false;
    bool m_ShowSearchPopup = false;
    ImVec2 m_ContextMenuPos = {};
    char m_SearchBuffer[128] = {};

    // 连线拖拽
    bool m_DraggingLink = false;
    u32 m_DragFromPinID = 0;
    ImVec2 m_DragEndPos = {};

    // 框选
    bool m_MarqueeActive = false;
    ImVec2 m_MarqueeStart = {};
    ImVec2 m_MarqueeEnd = {};

    // 小地图
    bool m_ShowMinimap = true;

    // 流动动画时间
    float m_FlowAnimTime = 0;
};

} // namespace Engine
