#pragma once

#include "engine/core/types.h"
#include "engine/core/scene.h"

#include <string>

namespace Engine {

// ── 场景序列化器 ────────────────────────────────────────────
// JSON 格式的场景保存/加载

class SceneSerializer {
public:
    /// 保存场景到 JSON 文件
    static bool Save(const Scene& scene, const std::string& filepath);

    /// 从 JSON 文件加载场景
    static Ref<Scene> Load(const std::string& filepath);
};

} // namespace Engine
