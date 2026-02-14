#include "engine/renderer/render_queue.h"

namespace Engine {

void RenderQueue::Submit(const RenderCommand& cmd) {
    if (cmd.Transparent) {
        m_TransparentCommands.push_back(cmd);
    } else {
        m_OpaqueCommands.push_back(cmd);
    }
}

void RenderQueue::Sort() {
    // 不透明: 按 SortKey (Shader→Material→Mesh), 同 key 按距离前到后
    std::sort(m_OpaqueCommands.begin(), m_OpaqueCommands.end(),
        [](const RenderCommand& a, const RenderCommand& b) {
            if (a.SortKey != b.SortKey) return a.SortKey < b.SortKey;
            return a.DistToCamera < b.DistToCamera;  // 前到后 (减少 overdraw)
        });

    // 透明: 后到前 (远→近, 正确混合)
    std::sort(m_TransparentCommands.begin(), m_TransparentCommands.end(),
        [](const RenderCommand& a, const RenderCommand& b) {
            return a.DistToCamera > b.DistToCamera;
        });
}

void RenderQueue::Clear() {
    m_OpaqueCommands.clear();
    m_TransparentCommands.clear();
}

u32 RenderQueue::CountBatches() const {
    u32 batches = 0;
    u64 lastKey = ~0ULL;
    for (auto& cmd : m_OpaqueCommands) {
        if (cmd.SortKey != lastKey) {
            batches++;
            lastKey = cmd.SortKey;
        }
    }
    if (!m_TransparentCommands.empty()) batches++;
    return batches;
}

} // namespace Engine
