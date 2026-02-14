#pragma once

#include "engine/renderer/animation.h"
#include "engine/renderer/animation_blend.h"

#include <vector>
#include <string>
#include <bitset>

namespace Engine {

// ── 骨骼遮罩 ────────────────────────────────────────────────

using BoneMask = std::bitset<MAX_BONES>;

/// 创建上半身遮罩（从指定骨骼开始向下传播）
BoneMask CreateBoneMaskFromRoot(const Skeleton& skeleton, const std::string& rootBoneName);

// ── 动画层 ──────────────────────────────────────────────────

enum class LayerBlendMode : u8 {
    Override,    // 覆盖（按 Weight 混合）
    Additive,    // 叠加
};

struct AnimLayer {
    std::string Name;
    std::string ClipName;                  // 此层播放的动画
    f32 Time   = 0.0f;                     // 当前播放时间
    f32 Speed  = 1.0f;
    bool Loop  = true;
    f32 Weight = 1.0f;                     // 混合权重
    BoneMask Mask;                         // 哪些骨骼受此层影响
    LayerBlendMode BlendMode = LayerBlendMode::Override;
    bool Active = true;
};

// ── 分层动画栈 ──────────────────────────────────────────────

class AnimLayerStack {
public:
    /// 设置基础层（全身动画）
    void SetBaseLayer(const std::string& clipName, bool loop = true, f32 speed = 1.0f);

    /// 添加叠加层
    void AddLayer(const AnimLayer& layer);

    /// 移除叠加层
    void RemoveLayer(const std::string& layerName);

    /// 设置层权重
    void SetLayerWeight(const std::string& layerName, f32 weight);

    /// 设置层动画
    void SetLayerClip(const std::string& layerName, const std::string& clipName);

    /// 更新所有层的时间
    void Update(f32 dt, const std::vector<AnimationClip>& clips);

    /// 合成最终姿势
    void ComputeFinalPose(const std::vector<AnimationClip>& clips,
                          const Skeleton& skeleton, AnimPose& outPose);

    /// 获取层数量
    u32 GetLayerCount() const { return (u32)(1 + m_OverlayLayers.size()); }

private:
    AnimLayer m_BaseLayer;
    std::vector<AnimLayer> m_OverlayLayers;

    const AnimationClip* FindClip(const std::vector<AnimationClip>& clips,
                                   const std::string& name) const;
};

} // namespace Engine
