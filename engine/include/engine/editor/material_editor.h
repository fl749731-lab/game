#pragma once

#include "engine/core/types.h"
#include "engine/editor/node_graph.h"

#include <imgui.h>
#include <string>
#include <vector>
#include <functional>

namespace Engine {

// ── 材质节点编辑器 ──────────────────────────────────────────
// 基于 NodeGraphEditor 的可视化材质编辑器
// 功能:
//   1. 预置材质节点 (颜色/纹理/法线/PBR参数/数学运算)
//   2. 输出节点 (连到 MaterialComponent)
//   3. 预览着色结果
//   4. 导出为 MaterialComponent 参数

struct MaterialNodeType {
    enum Value : u8 {
        Output,     // 最终输出
        Color,      // 常量颜色
        Texture,    // 纹理采样
        Normal,     // 法线贴图
        Float,      // 常量浮点
        Multiply,   // 乘法
        Add,        // 加法
        Lerp,       // 线性插值
        Fresnel,    // 菲涅尔
        UV,         // UV 坐标
        Time,       // 时间输入
    };
};

class MaterialEditor {
public:
    MaterialEditor();
    ~MaterialEditor();

    void Render(const char* title = "材质编辑器");

    /// 创建默认节点布局
    void CreateDefaultLayout();

    /// 导出到 MaterialComponent 参数
    struct MaterialParams {
        f32 DiffuseR = 0.8f, DiffuseG = 0.8f, DiffuseB = 0.8f;
        f32 SpecularR = 0.8f, SpecularG = 0.8f, SpecularB = 0.8f;
        f32 Roughness = 0.5f, Metallic = 0.0f, Shininess = 32.0f;
        std::string TextureName;
        std::string NormalMapName;
        bool Emissive = false;
        f32 EmissiveR = 1, EmissiveG = 1, EmissiveB = 1;
        f32 EmissiveIntensity = 1.0f;
    };
    MaterialParams ExportParams() const;

    /// 设置预览回调
    using PreviewCallback = std::function<void(const MaterialParams&)>;
    void SetPreviewCallback(PreviewCallback cb) { m_PreviewCallback = cb; }

private:
    void RegisterMaterialNodes();
    void RenderPreview();

    NodeGraphEditor m_Graph;
    PreviewCallback m_PreviewCallback;

    // 节点中的参数缓存
    struct ColorNodeData { f32 R = 0.8f, G = 0.8f, B = 0.8f; };
    struct FloatNodeData { f32 Value = 0.5f; };
    struct TextureNodeData { char Path[128] = {}; };

    std::vector<ColorNodeData> m_ColorDatas;
    std::vector<FloatNodeData> m_FloatDatas;
    std::vector<TextureNodeData> m_TextureDatas;
};

} // namespace Engine
