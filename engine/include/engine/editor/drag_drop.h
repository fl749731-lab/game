#pragma once

#include "engine/core/types.h"
#include <imgui.h>
#include <string>
#include <functional>

namespace Engine {

// ── 拖放系统 ────────────────────────────────────────────────
// Asset Browser → Scene / Inspector 的拖放桥接
// 功能:
//   1. 定义拖放载荷类型
//   2. 拖放源标记 (Asset Browser / Hierarchy)
//   3. 拖放目标接受 (Scene View / Inspector)

struct DragDropPayload {
    enum Type : u8 {
        None = 0,
        AssetPath,     // 文件路径 (纹理/模型/脚本)
        EntityRef,     // 实体 ID
        MaterialParam, // 材质参数拖拽
    };

    Type PayloadType = None;
    char Path[256] = {};
    u32  EntityID = ~u32(0);  // INVALID_ENTITY
};

class DragDropManager {
public:
    static void Init();
    static void Shutdown();

    // ImGui 拖放源
    static bool BeginSource(const char* type, const DragDropPayload& payload);
    static void EndSource();

    // ImGui 拖放目标
    static const DragDropPayload* AcceptTarget(const char* type);

    // 注册文件类型处理器
    using FileHandler = std::function<void(const std::string& path)>;
    static void RegisterFileHandler(const std::string& extension, FileHandler handler);

    // 处理拖入的文件
    static void HandleFileDrop(const std::string& path);

    /// 渲染拖拽悬浮预览
    static void RenderDragPreview();

    // 标准拖放类型字符串
    static constexpr const char* TYPE_ASSET   = "ASSET_PATH";
    static constexpr const char* TYPE_ENTITY  = "ENTITY_REF";
    static constexpr const char* TYPE_TEXTURE = "TEXTURE_PATH";
    static constexpr const char* TYPE_MODEL   = "MODEL_PATH";

private:
    inline static std::unordered_map<std::string, FileHandler> s_Handlers;
    inline static DragDropPayload s_CurrentPayload;
};

} // namespace Engine
