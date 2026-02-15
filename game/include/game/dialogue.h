#pragma once

#include "engine/core/types.h"
#include "engine/core/ecs.h"

#include <string>
#include <vector>
#include <functional>

namespace Engine {

// ── 对话选项 ──────────────────────────────────────────────

struct DialogueChoice {
    std::string Text;
    std::string NextNodeID;
    std::string ConditionKey;
};

// ── 对话节点 ──────────────────────────────────────────────

struct DialogueNode {
    std::string ID;
    std::string Speaker;
    std::string Portrait;
    std::string Text;
    std::vector<DialogueChoice> Choices;
    std::string NextNodeID;
};

// ── 对话树 ────────────────────────────────────────────────

class DialogueTree {
public:
    bool LoadFromJSON(const std::string& filepath);
    const DialogueNode* GetStartNode() const;
    const DialogueNode* GetNode(const std::string& id) const;
    void AddNode(const DialogueNode& node);
    void SetStartNodeID(const std::string& id) { m_StartNodeID = id; }

private:
    std::vector<DialogueNode> m_Nodes;
    std::string m_StartNodeID;
};

// ── 对话状态 ──────────────────────────────────────────────

enum class DialogueState : u8 {
    Inactive = 0,
    Typing,
    WaitingInput,
    Choosing,
    Finished,
};

// ── 对话控制器 ────────────────────────────────────────────

class DialogueController {
public:
    void Start(const DialogueTree& tree);
    void Update(f32 dt);
    void Advance();
    void SelectChoice(u32 index);
    void SkipTyping();

    DialogueState GetState() const { return m_State; }
    bool IsActive() const { return m_State != DialogueState::Inactive && m_State != DialogueState::Finished; }
    const DialogueNode* GetCurrentNode() const { return m_CurrentNode; }
    const std::string& GetDisplayText() const { return m_DisplayText; }
    bool IsTextComplete() const { return m_CharIndex >= m_FullText.size(); }
    void SetTypingSpeed(f32 charsPerSec) { m_TypingSpeed = charsPerSec; }

private:
    const DialogueTree* m_Tree = nullptr;
    const DialogueNode* m_CurrentNode = nullptr;
    DialogueState m_State = DialogueState::Inactive;
    std::string m_FullText;
    std::string m_DisplayText;
    u32  m_CharIndex = 0;
    f32  m_TypingSpeed = 30.0f;
    f32  m_TypingTimer = 0.0f;
};

} // namespace Engine
