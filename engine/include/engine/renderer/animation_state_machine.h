#pragma once

#include "engine/renderer/animation.h"
#include "engine/renderer/animation_blend.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Engine {

// ── 过渡条件 ────────────────────────────────────────────────

enum class TransitionConditionType : u8 {
    BoolTrue,      // 布尔参数为 true
    BoolFalse,     // 布尔参数为 false
    FloatGreater,  // 浮点参数 > 阈值
    FloatLess,     // 浮点参数 < 阈值
    Trigger,       // 触发器（触发后自动重置）
    AnimFinished,  // 当前动画播放完毕
};

struct TransitionCondition {
    TransitionConditionType Type;
    std::string ParameterName;
    f32 Threshold = 0.0f;
};

// ── 动画过渡 ────────────────────────────────────────────────

struct AnimTransition {
    std::string TargetState;
    f32 Duration = 0.2f;               // 过渡时长（秒）
    std::vector<TransitionCondition> Conditions;  // 所有条件 AND
};

// ── 动画状态 ────────────────────────────────────────────────

struct AnimState {
    std::string Name;
    std::string ClipName;              // 对应的 AnimationClip 名称
    f32 Speed = 1.0f;
    bool Loop = true;
    std::vector<AnimTransition> Transitions;
};

// ── 动画状态机 ──────────────────────────────────────────────

class AnimStateMachine {
public:
    /// 添加状态
    void AddState(const AnimState& state);

    /// 设置初始状态
    void SetEntryState(const std::string& stateName);

    /// 设置布尔参数
    void SetBool(const std::string& name, bool value);

    /// 设置浮点参数
    void SetFloat(const std::string& name, f32 value);

    /// 触发触发器
    void SetTrigger(const std::string& name);

    /// 获取参数值
    bool GetBool(const std::string& name) const;
    f32 GetFloat(const std::string& name) const;

    /// 更新状态机（检查过渡 + 驱动 Crossfade）
    void Update(f32 dt, const AnimatorComponent& animator);

    /// 获取当前状态名
    const std::string& GetCurrentStateName() const { return m_CurrentState; }

    /// 获取当前应播放的 Clip 名称
    const std::string& GetCurrentClipName() const;

    /// 获取 Crossfade 引用
    const Crossfade& GetCrossfade() const { return m_Crossfade; }

private:
    bool EvaluateCondition(const TransitionCondition& cond,
                           const AnimatorComponent& animator) const;

    std::unordered_map<std::string, AnimState> m_States;
    std::string m_CurrentState;
    std::string m_PreviousState;

    // 参数
    std::unordered_map<std::string, bool> m_BoolParams;
    std::unordered_map<std::string, f32>  m_FloatParams;
    std::unordered_map<std::string, bool> m_Triggers; // true=已触发

    Crossfade m_Crossfade;
};

} // namespace Engine
