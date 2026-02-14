#include "engine/renderer/animation_state_machine.h"
#include "engine/core/log.h"

namespace Engine {

void AnimStateMachine::AddState(const AnimState& state) {
    m_States[state.Name] = state;
}

void AnimStateMachine::SetEntryState(const std::string& stateName) {
    m_CurrentState = stateName;
}

void AnimStateMachine::SetBool(const std::string& name, bool value) {
    m_BoolParams[name] = value;
}

void AnimStateMachine::SetFloat(const std::string& name, f32 value) {
    m_FloatParams[name] = value;
}

void AnimStateMachine::SetTrigger(const std::string& name) {
    m_Triggers[name] = true;
}

bool AnimStateMachine::GetBool(const std::string& name) const {
    auto it = m_BoolParams.find(name);
    return it != m_BoolParams.end() ? it->second : false;
}

f32 AnimStateMachine::GetFloat(const std::string& name) const {
    auto it = m_FloatParams.find(name);
    return it != m_FloatParams.end() ? it->second : 0.0f;
}

bool AnimStateMachine::EvaluateCondition(const TransitionCondition& cond,
                                          const AnimatorComponent& animator) const {
    switch (cond.Type) {
        case TransitionConditionType::BoolTrue:
            return GetBool(cond.ParameterName);
        case TransitionConditionType::BoolFalse:
            return !GetBool(cond.ParameterName);
        case TransitionConditionType::FloatGreater:
            return GetFloat(cond.ParameterName) > cond.Threshold;
        case TransitionConditionType::FloatLess:
            return GetFloat(cond.ParameterName) < cond.Threshold;
        case TransitionConditionType::Trigger: {
            auto it = m_Triggers.find(cond.ParameterName);
            return it != m_Triggers.end() && it->second;
        }
        case TransitionConditionType::AnimFinished:
            // 非循环动画且时间到头
            return !animator.Loop && animator.CurrentTime >= 0.99f;
    }
    return false;
}

void AnimStateMachine::Update(f32 dt, const AnimatorComponent& animator) {
    if (m_CurrentState.empty()) return;
    if (m_Crossfade.IsActive()) {
        m_Crossfade.Update(dt);
        if (!m_Crossfade.IsActive()) {
            // 过渡完成，正式切换
            m_CurrentState = m_Crossfade.GetToClip();
        }
        return;
    }

    auto it = m_States.find(m_CurrentState);
    if (it == m_States.end()) return;

    const AnimState& state = it->second;

    // 检查所有过渡
    for (auto& trans : state.Transitions) {
        bool allMet = true;
        for (auto& cond : trans.Conditions) {
            if (!EvaluateCondition(cond, animator)) {
                allMet = false;
                break;
            }
        }
        if (allMet) {
            // 消耗触发器
            for (auto& cond : trans.Conditions) {
                if (cond.Type == TransitionConditionType::Trigger) {
                    m_Triggers[cond.ParameterName] = false;
                }
            }

            // 启动 crossfade
            auto targetIt = m_States.find(trans.TargetState);
            if (targetIt != m_States.end()) {
                m_PreviousState = m_CurrentState;
                m_Crossfade.Start(state.ClipName, targetIt->second.ClipName, trans.Duration);
                LOG_DEBUG("[AnimFSM] 过渡: %s → %s (%.2fs)",
                          m_CurrentState.c_str(), trans.TargetState.c_str(), trans.Duration);
            }
            break; // 只触发优先级最高的过渡
        }
    }
}

const std::string& AnimStateMachine::GetCurrentClipName() const {
    static const std::string empty;
    auto it = m_States.find(m_CurrentState);
    return it != m_States.end() ? it->second.ClipName : empty;
}

} // namespace Engine
