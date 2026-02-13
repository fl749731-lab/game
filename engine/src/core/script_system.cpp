#ifdef ENGINE_HAS_PYTHON

#include "engine/core/script_system.h"
#include "engine/ai/python_engine.h"
#include "engine/core/log.h"
#include "engine/core/event.h"

#include <sstream>

namespace Engine {

// ── ScriptSystem::Update ────────────────────────────────────

void ScriptSystem::Update(ECSWorld& world, f32 dt) {
    if (!AI::PythonEngine::IsInitialized()) return;

    world.ForEach<ScriptComponent>([&](Entity e, ScriptComponent& sc) {
        if (!sc.Enabled || sc.ScriptModule.empty()) return;

        // 首次执行 → on_create
        if (!sc.Initialized) {
            std::string ctxJson = BuildEntityContext(world, e);
            AI::PythonEngine::CallFunction(sc.ScriptModule, "on_create",
                                           {std::to_string(e), ctxJson});
            sc.Initialized = true;
        }

        // 每帧 → on_update
        std::string ctxJson = BuildEntityContext(world, e);
        std::string result = AI::PythonEngine::CallFunction(
            sc.ScriptModule, "on_update",
            {std::to_string(e), std::to_string(dt), ctxJson});

        // 解析返回值并应用 (如果有)
        if (!result.empty() && result != "None") {
            // 简单命令解析: "cmd:arg1,arg2,..."
            // 由 Python 脚本通过 engine_api 直接调用 C++ 更高效
            // 这里作为备用通道
        }
    });
}

// ── SendEvent ───────────────────────────────────────────────

void ScriptSystem::SendEvent(ECSWorld& world, Entity e,
                              const std::string& eventType,
                              const std::string& eventData) {
    auto* sc = world.GetComponent<ScriptComponent>(e);
    if (!sc || !sc->Enabled || sc->ScriptModule.empty()) return;
    if (!AI::PythonEngine::IsInitialized()) return;

    std::string eventJson = "{\"type\":\"" + eventType + "\"";
    if (!eventData.empty()) {
        eventJson += ",\"data\":" + eventData;
    }
    eventJson += "}";

    AI::PythonEngine::CallFunction(sc->ScriptModule, "on_event",
                                    {std::to_string(e), eventJson});
}

// ── NotifyDestroy ───────────────────────────────────────────

void ScriptSystem::NotifyDestroy(ECSWorld& world, Entity e) {
    auto* sc = world.GetComponent<ScriptComponent>(e);
    if (!sc || !sc->Initialized || sc->ScriptModule.empty()) return;
    if (!AI::PythonEngine::IsInitialized()) return;

    AI::PythonEngine::CallFunction(sc->ScriptModule, "on_destroy",
                                    {std::to_string(e)});
}

// ── BuildEntityContext ──────────────────────────────────────

std::string ScriptSystem::BuildEntityContext(ECSWorld& world, Entity e) {
    std::ostringstream ss;
    ss << "{";
    ss << "\"entity_id\":" << e;

    // Tag
    if (auto* tag = world.GetComponent<TagComponent>(e)) {
        ss << ",\"name\":\"" << tag->Name << "\"";
    }

    // Transform
    if (auto* tr = world.GetComponent<TransformComponent>(e)) {
        ss << ",\"pos\":[" << tr->X << "," << tr->Y << "," << tr->Z << "]";
        ss << ",\"rot\":[" << tr->RotX << "," << tr->RotY << "," << tr->RotZ << "]";
        ss << ",\"scale\":[" << tr->ScaleX << "," << tr->ScaleY << "," << tr->ScaleZ << "]";
    }

    // Health
    if (auto* hp = world.GetComponent<HealthComponent>(e)) {
        ss << ",\"health\":" << hp->Current;
        ss << ",\"max_health\":" << hp->Max;
    }

    // ScriptComponent 自定义变量
    auto* sc = world.GetComponent<ScriptComponent>(e);
    if (sc) {
        if (!sc->FloatVars.empty()) {
            ss << ",\"float_vars\":{";
            bool first = true;
            for (auto& [k, v] : sc->FloatVars) {
                if (!first) ss << ",";
                ss << "\"" << k << "\":" << v;
                first = false;
            }
            ss << "}";
        }
        if (!sc->StringVars.empty()) {
            ss << ",\"string_vars\":{";
            bool first = true;
            for (auto& [k, v] : sc->StringVars) {
                if (!first) ss << ",";
                ss << "\"" << k << "\":\"" << v << "\"";
                first = false;
            }
            ss << "}";
        }
    }

    ss << "}";
    return ss.str();
}

} // namespace Engine

#else // !ENGINE_HAS_PYTHON

// ── 无 Python 时的 stub 实现 ─────────────────────────────────

#include "engine/core/script_system.h"
#include "engine/core/log.h"

namespace Engine {

void ScriptSystem::Update(ECSWorld& world, f32 dt) {
    (void)world; (void)dt;
    // Python 未启用时，ScriptSystem 不执行任何操作
}

void ScriptSystem::SendEvent(ECSWorld& world, Entity e,
                              const std::string& eventType,
                              const std::string& eventData) {
    (void)world; (void)e; (void)eventType; (void)eventData;
}

void ScriptSystem::NotifyDestroy(ECSWorld& world, Entity e) {
    (void)world; (void)e;
}

std::string ScriptSystem::BuildEntityContext(ECSWorld& world, Entity e) {
    (void)world; (void)e;
    return "{}";
}

} // namespace Engine

#endif // ENGINE_HAS_PYTHON
