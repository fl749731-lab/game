#include "engine/renderer/animation_layer.h"
#include "engine/core/log.h"

#include <algorithm>
#include <queue>

namespace Engine {

// ═══════════════════════════════════════════════════════════
//  BoneMask 工具
// ═══════════════════════════════════════════════════════════

BoneMask CreateBoneMaskFromRoot(const Skeleton& skeleton, const std::string& rootBoneName) {
    BoneMask mask;
    i32 rootIdx = skeleton.FindBone(rootBoneName);
    if (rootIdx < 0) {
        LOG_WARN("[AnimLayer] 找不到骨骼: %s", rootBoneName.c_str());
        return mask;
    }

    // BFS: 从 rootBone 开始，标记所有子骨骼
    std::queue<i32> q;
    q.push(rootIdx);
    while (!q.empty()) {
        i32 idx = q.front(); q.pop();
        if (idx >= 0 && idx < (i32)MAX_BONES) {
            mask.set(idx);
        }
        // 找所有以 idx 为父骨骼的子骨骼
        for (u32 i = 0; i < skeleton.GetBoneCount(); i++) {
            if (skeleton.GetBone(i).ParentIndex == idx && !mask.test(i)) {
                q.push(i);
            }
        }
    }
    return mask;
}

// ═══════════════════════════════════════════════════════════
//  AnimLayerStack
// ═══════════════════════════════════════════════════════════

void AnimLayerStack::SetBaseLayer(const std::string& clipName, bool loop, f32 speed) {
    m_BaseLayer.Name     = "Base";
    m_BaseLayer.ClipName = clipName;
    m_BaseLayer.Loop     = loop;
    m_BaseLayer.Speed    = speed;
    m_BaseLayer.Weight   = 1.0f;
    m_BaseLayer.Active   = true;
    m_BaseLayer.Mask.set(); // 全部骨骼
}

void AnimLayerStack::AddLayer(const AnimLayer& layer) {
    m_OverlayLayers.push_back(layer);
}

void AnimLayerStack::RemoveLayer(const std::string& layerName) {
    m_OverlayLayers.erase(
        std::remove_if(m_OverlayLayers.begin(), m_OverlayLayers.end(),
            [&](const AnimLayer& l) { return l.Name == layerName; }),
        m_OverlayLayers.end());
}

void AnimLayerStack::SetLayerWeight(const std::string& layerName, f32 weight) {
    for (auto& l : m_OverlayLayers) {
        if (l.Name == layerName) {
            l.Weight = glm::clamp(weight, 0.0f, 1.0f);
            return;
        }
    }
}

void AnimLayerStack::SetLayerClip(const std::string& layerName, const std::string& clipName) {
    for (auto& l : m_OverlayLayers) {
        if (l.Name == layerName) {
            l.ClipName = clipName;
            l.Time = 0.0f;
            return;
        }
    }
}

const AnimationClip* AnimLayerStack::FindClip(const std::vector<AnimationClip>& clips,
                                                const std::string& name) const {
    for (auto& c : clips) {
        if (c.Name == name) return &c;
    }
    return nullptr;
}

void AnimLayerStack::Update(f32 dt, const std::vector<AnimationClip>& clips) {
    // 基础层时间推进
    auto* baseClip = FindClip(clips, m_BaseLayer.ClipName);
    if (baseClip && m_BaseLayer.Active) {
        m_BaseLayer.Time += dt * m_BaseLayer.Speed;
        if (m_BaseLayer.Loop && baseClip->Duration > 0)
            m_BaseLayer.Time = fmod(m_BaseLayer.Time, baseClip->Duration);
    }

    // 叠加层时间推进
    for (auto& layer : m_OverlayLayers) {
        if (!layer.Active) continue;
        auto* clip = FindClip(clips, layer.ClipName);
        if (!clip) continue;
        layer.Time += dt * layer.Speed;
        if (layer.Loop && clip->Duration > 0)
            layer.Time = fmod(layer.Time, clip->Duration);
    }
}

void AnimLayerStack::ComputeFinalPose(const std::vector<AnimationClip>& clips,
                                       const Skeleton& skeleton, AnimPose& outPose) {
    u32 boneCount = skeleton.GetBoneCount();
    outPose.Resize(boneCount);

    // 1) 采样基础层
    auto* baseClip = FindClip(clips, m_BaseLayer.ClipName);
    if (baseClip) {
        PoseSampler::SampleClip(*baseClip, m_BaseLayer.Time, skeleton, outPose);
    }

    // 2) 叠加各层
    AnimPose layerPose;
    for (auto& layer : m_OverlayLayers) {
        if (!layer.Active || layer.Weight < 0.001f) continue;
        auto* clip = FindClip(clips, layer.ClipName);
        if (!clip) continue;

        PoseSampler::SampleClip(*clip, layer.Time, skeleton, layerPose);

        // 按遮罩混合
        for (u32 i = 0; i < boneCount; i++) {
            if (!layer.Mask.test(i)) continue;

            if (layer.BlendMode == LayerBlendMode::Override) {
                outPose.BonePoses[i].Position = glm::mix(
                    outPose.BonePoses[i].Position, layerPose.BonePoses[i].Position, layer.Weight);
                outPose.BonePoses[i].Rotation = glm::slerp(
                    outPose.BonePoses[i].Rotation, layerPose.BonePoses[i].Rotation, layer.Weight);
                outPose.BonePoses[i].Scale = glm::mix(
                    outPose.BonePoses[i].Scale, layerPose.BonePoses[i].Scale, layer.Weight);
            }
            // Additive 模式留给 PoseBlender::BlendAdditive
        }
    }
}

} // namespace Engine
