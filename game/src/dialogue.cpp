#include "game/dialogue.h"
#include "engine/core/log.h"

namespace Engine {

bool DialogueTree::LoadFromJSON(const std::string& filepath) {
    LOG_WARN("[对话] JSON 加载暂未实现: %s", filepath.c_str());
    return false;
}

const DialogueNode* DialogueTree::GetStartNode() const {
    return GetNode(m_StartNodeID);
}

const DialogueNode* DialogueTree::GetNode(const std::string& id) const {
    for (auto& node : m_Nodes) if (node.ID == id) return &node;
    return nullptr;
}

void DialogueTree::AddNode(const DialogueNode& node) {
    m_Nodes.push_back(node);
}

void DialogueController::Start(const DialogueTree& tree) {
    m_Tree = &tree;
    m_CurrentNode = tree.GetStartNode();
    if (!m_CurrentNode) { m_State = DialogueState::Finished; return; }
    m_FullText = m_CurrentNode->Text;
    m_DisplayText.clear();
    m_CharIndex = 0;
    m_TypingTimer = 0.0f;
    m_State = DialogueState::Typing;
}

void DialogueController::Update(f32 dt) {
    if (m_State != DialogueState::Typing) return;
    m_TypingTimer += dt;
    f32 interval = 1.0f / m_TypingSpeed;
    while (m_TypingTimer >= interval && m_CharIndex < m_FullText.size()) {
        m_TypingTimer -= interval;
        u8 c = (u8)m_FullText[m_CharIndex];
        u32 charLen = 1;
        if (c >= 0xF0)      charLen = 4;
        else if (c >= 0xE0) charLen = 3;
        else if (c >= 0xC0) charLen = 2;
        for (u32 i = 0; i < charLen && m_CharIndex < m_FullText.size(); i++)
            m_DisplayText += m_FullText[m_CharIndex++];
    }
    if (m_CharIndex >= m_FullText.size()) {
        m_State = (m_CurrentNode && !m_CurrentNode->Choices.empty())
                  ? DialogueState::Choosing : DialogueState::WaitingInput;
    }
}

void DialogueController::Advance() {
    if (m_State == DialogueState::Typing) { SkipTyping(); return; }
    if (m_State != DialogueState::WaitingInput) return;
    if (!m_CurrentNode || !m_Tree || m_CurrentNode->NextNodeID.empty()) {
        m_State = DialogueState::Finished; return;
    }
    m_CurrentNode = m_Tree->GetNode(m_CurrentNode->NextNodeID);
    if (!m_CurrentNode) { m_State = DialogueState::Finished; return; }
    m_FullText = m_CurrentNode->Text;
    m_DisplayText.clear();
    m_CharIndex = 0;
    m_TypingTimer = 0.0f;
    m_State = DialogueState::Typing;
}

void DialogueController::SelectChoice(u32 index) {
    if (m_State != DialogueState::Choosing) return;
    if (!m_CurrentNode || index >= m_CurrentNode->Choices.size()) return;
    const auto& choice = m_CurrentNode->Choices[index];
    if (choice.NextNodeID.empty()) { m_State = DialogueState::Finished; return; }
    m_CurrentNode = m_Tree->GetNode(choice.NextNodeID);
    if (!m_CurrentNode) { m_State = DialogueState::Finished; return; }
    m_FullText = m_CurrentNode->Text;
    m_DisplayText.clear();
    m_CharIndex = 0;
    m_TypingTimer = 0.0f;
    m_State = DialogueState::Typing;
}

void DialogueController::SkipTyping() {
    if (m_State != DialogueState::Typing) return;
    m_DisplayText = m_FullText;
    m_CharIndex = (u32)m_FullText.size();
    m_State = (m_CurrentNode && !m_CurrentNode->Choices.empty())
              ? DialogueState::Choosing : DialogueState::WaitingInput;
}

} // namespace Engine
