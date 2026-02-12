#ifdef ENGINE_HAS_PYTHON

#include "engine/ai/python_engine.h"
#include "engine/core/scene.h"
#include "engine/core/ecs.h"

#include <Python.h>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace Engine {
namespace AI {

// ════════════════════════════════════════════════════════════
// PythonEngine
// ════════════════════════════════════════════════════════════

bool PythonEngine::s_Initialized = false;
std::string PythonEngine::s_LastError;

bool PythonEngine::Init(const std::string& scriptsPath) {
    if (s_Initialized) {
        LOG_WARN("[AI] Python 引擎已经初始化");
        return true;
    }

    LOG_INFO("[AI] 正在初始化 Python 解释器...");
    Py_Initialize();

    if (!Py_IsInitialized()) {
        s_LastError = "Python 解释器初始化失败";
        LOG_ERROR("[AI] %s", s_LastError.c_str());
        return false;
    }

    // 添加脚本路径
    std::string pathCmd = "import sys; sys.path.insert(0, '" + scriptsPath + "')";
    PyRun_SimpleString(pathCmd.c_str());

    s_Initialized = true;
    const char* version = Py_GetVersion();
    LOG_INFO("[AI] Python %s 已就绪, 脚本: %s", version, scriptsPath.c_str());
    return true;
}

void PythonEngine::Shutdown() {
    if (!s_Initialized) return;
    LOG_INFO("[AI] 关闭 Python 解释器...");
    Py_Finalize();
    s_Initialized = false;
}

bool PythonEngine::IsInitialized() { return s_Initialized; }

bool PythonEngine::Execute(const std::string& code) {
    if (!s_Initialized) { s_LastError = "Python 未初始化"; return false; }
    int result = PyRun_SimpleString(code.c_str());
    if (result != 0) {
        s_LastError = "Python 执行失败";
        LOG_ERROR("[AI] %s: %s", s_LastError.c_str(), code.c_str());
        return false;
    }
    return true;
}

bool PythonEngine::ExecuteFile(const std::string& filepath) {
    if (!s_Initialized) { s_LastError = "Python 未初始化"; return false; }
    FILE* fp = fopen(filepath.c_str(), "r");
    if (!fp) {
        s_LastError = "无法打开: " + filepath;
        LOG_ERROR("[AI] %s", s_LastError.c_str());
        return false;
    }
    int result = PyRun_SimpleFile(fp, filepath.c_str());
    fclose(fp);
    if (result != 0) {
        s_LastError = "脚本失败: " + filepath;
        LOG_ERROR("[AI] %s", s_LastError.c_str());
        return false;
    }
    return true;
}

std::string PythonEngine::CallFunction(const std::string& module,
                                        const std::string& func,
                                        const std::vector<std::string>& args) {
    if (!s_Initialized) { s_LastError = "Python 未初始化"; return ""; }

    PyObject* pModuleName = PyUnicode_FromString(module.c_str());
    PyObject* pModule = PyImport_Import(pModuleName);
    Py_DECREF(pModuleName);

    if (!pModule) {
        PyErr_Print();
        s_LastError = "无法导入: " + module;
        LOG_ERROR("[AI] %s", s_LastError.c_str());
        return "";
    }

    PyObject* pFunc = PyObject_GetAttrString(pModule, func.c_str());
    if (!pFunc || !PyCallable_Check(pFunc)) {
        PyErr_Print();
        Py_XDECREF(pFunc);
        Py_DECREF(pModule);
        s_LastError = "找不到函数: " + module + "." + func;
        LOG_ERROR("[AI] %s", s_LastError.c_str());
        return "";
    }

    PyObject* pArgs = PyTuple_New((Py_ssize_t)args.size());
    for (size_t i = 0; i < args.size(); i++) {
        PyTuple_SetItem(pArgs, (Py_ssize_t)i, PyUnicode_FromString(args[i].c_str()));
    }

    PyObject* pResult = PyObject_CallObject(pFunc, pArgs);
    Py_DECREF(pArgs);
    Py_DECREF(pFunc);
    Py_DECREF(pModule);

    if (!pResult) {
        PyErr_Print();
        s_LastError = "调用失败: " + module + "." + func;
        LOG_ERROR("[AI] %s", s_LastError.c_str());
        return "";
    }

    std::string result;
    PyObject* pStr = PyObject_Str(pResult);
    if (pStr) {
        const char* str = PyUnicode_AsUTF8(pStr);
        if (str) result = str;
        Py_DECREF(pStr);
    }
    Py_DECREF(pResult);
    return result;
}

std::string PythonEngine::GetVariable(const std::string& module,
                                       const std::string& varName) {
    if (!s_Initialized) return "";
    PyObject* pModuleName = PyUnicode_FromString(module.c_str());
    PyObject* pModule = PyImport_Import(pModuleName);
    Py_DECREF(pModuleName);
    if (!pModule) { PyErr_Print(); return ""; }

    PyObject* pVar = PyObject_GetAttrString(pModule, varName.c_str());
    Py_DECREF(pModule);
    if (!pVar) { PyErr_Print(); return ""; }

    std::string result;
    PyObject* pStr = PyObject_Str(pVar);
    if (pStr) {
        const char* str = PyUnicode_AsUTF8(pStr);
        if (str) result = str;
        Py_DECREF(pStr);
    }
    Py_DECREF(pVar);
    return result;
}

std::string PythonEngine::GetLastError() { return s_LastError; }

// ════════════════════════════════════════════════════════════
// AIState 转换
// ════════════════════════════════════════════════════════════

const char* AIStateToString(AIState state) {
    switch (state) {
        case AIState::Idle:   return "Idle";
        case AIState::Patrol: return "Patrol";
        case AIState::Chase:  return "Chase";
        case AIState::Attack: return "Attack";
        case AIState::Flee:   return "Flee";
        case AIState::Dead:   return "Dead";
        default:              return "Unknown";
    }
}

AIState AIStateFromString(const std::string& str) {
    if (str == "Idle")    return AIState::Idle;
    if (str == "Patrol")  return AIState::Patrol;
    if (str == "Chase")   return AIState::Chase;
    if (str == "Attack")  return AIState::Attack;
    if (str == "Flee")    return AIState::Flee;
    if (str == "Dead")    return AIState::Dead;
    return AIState::Idle;
}

// ════════════════════════════════════════════════════════════
// AIAgent
// ════════════════════════════════════════════════════════════

AIAction AIAgent::UpdateAI(const AIContext& ctx) {
    AIAction action;
    action.NewState = ctx.CurrentState;

    if (!PythonEngine::IsInitialized()) return action;

    // 序列化上下文为 JSON 字符串传给 Python
    std::ostringstream ss;
    ss << "{";
    ss << "\"entity_id\":" << ctx.EntityID << ",";
    ss << "\"pos\":[" << ctx.Position.x << "," << ctx.Position.y << "," << ctx.Position.z << "],";
    ss << "\"health\":" << ctx.Health << ",";
    ss << "\"max_health\":" << ctx.MaxHealth << ",";
    ss << "\"state\":\"" << AIStateToString(ctx.CurrentState) << "\",";
    ss << "\"detect_range\":" << ctx.DetectRange << ",";
    ss << "\"attack_range\":" << ctx.AttackRange << ",";
    ss << "\"move_speed\":" << ctx.MoveSpeed << ",";
    ss << "\"dt\":" << ctx.DeltaTime << ",";

    // 附近敌人
    ss << "\"enemies\":[";
    for (size_t i = 0; i < ctx.NearbyEnemies.size(); i++) {
        auto& e = ctx.NearbyEnemies[i];
        if (i > 0) ss << ",";
        ss << "{\"id\":" << e.EntityID
           << ",\"pos\":[" << e.Position.x << "," << e.Position.y << "," << e.Position.z << "]"
           << ",\"health\":" << e.Health
           << ",\"dist\":" << e.Distance
           << ",\"tag\":\"" << e.Tag << "\""
           << "}";
    }
    ss << "],";

    // 巡逻点
    ss << "\"patrol_points\":[";
    for (size_t i = 0; i < ctx.PatrolPoints.size(); i++) {
        if (i > 0) ss << ",";
        ss << "[" << ctx.PatrolPoints[i].x << "," << ctx.PatrolPoints[i].y << "," << ctx.PatrolPoints[i].z << "]";
    }
    ss << "],";
    ss << "\"patrol_index\":" << ctx.CurrentPatrolIndex;
    ss << "}";

    std::string ctxJson = ss.str();
    std::string result = PythonEngine::CallFunction(ScriptModule, "update_ai", {ctxJson});

    // 解析返回值: "state|dir_x,dir_y,dir_z|speed|target_id|custom"
    action = AIManager::ParseAction(result);
    return action;
}

// ════════════════════════════════════════════════════════════
// AIManager
// ════════════════════════════════════════════════════════════

u32 AIManager::s_AgentCount = 0;

void AIManager::Init() {
    s_AgentCount = 0;
    LOG_INFO("[AI] AIManager 已初始化");
}

void AIManager::Shutdown() {
    s_AgentCount = 0;
    LOG_DEBUG("[AI] AIManager 已关闭");
}

void AIManager::Update(Scene& scene, f32 dt) {
    if (!PythonEngine::IsInitialized()) return;

    auto& world = scene.GetWorld();
    u32 count = 0;

    for (auto e : world.GetEntities()) {
        auto* aiComp = world.GetComponent<AIComponent>(e);
        if (!aiComp) continue;

        auto* hpComp = world.GetComponent<HealthComponent>(e);
        auto* trComp = world.GetComponent<TransformComponent>(e);

        // 跳过死亡实体
        if (hpComp && hpComp->Current <= 0) continue;

        // 构建上下文
        AIContext ctx = BuildContext(scene, e, dt);

        // 用 AIAgent 包装调用 Python
        AIAgent agent;
        agent.EntityID = e;
        agent.State = AIStateFromString(aiComp->State);
        agent.DetectRange = aiComp->DetectRange;
        agent.AttackRange = aiComp->AttackRange;
        agent.ScriptModule = aiComp->ScriptModule;

        AIAction action = agent.UpdateAI(ctx);

        // 回写状态
        aiComp->State = AIStateToString(action.NewState);

        // 应用移动
        ApplyAction(scene, e, action, dt);

        count++;
    }

    s_AgentCount = count;
}

AIContext AIManager::BuildContext(Scene& scene, u32 entityID, f32 dt) {
    AIContext ctx;
    ctx.EntityID = entityID;
    ctx.DeltaTime = dt;

    auto& world = scene.GetWorld();

    // 位置
    if (auto* tr = world.GetComponent<TransformComponent>(entityID)) {
        ctx.Position = {tr->X, tr->Y, tr->Z};
        ctx.Rotation = {tr->RotX, tr->RotY, tr->RotZ};
    }

    // 生命
    if (auto* hp = world.GetComponent<HealthComponent>(entityID)) {
        ctx.Health = hp->Current;
        ctx.MaxHealth = hp->Max;
    }

    // AI 参数
    if (auto* ai = world.GetComponent<AIComponent>(entityID)) {
        ctx.CurrentState = AIStateFromString(ai->State);
        ctx.DetectRange = ai->DetectRange;
        ctx.AttackRange = ai->AttackRange;
    }

    // 查找附近实体
    ctx.NearbyEnemies = FindNearbyEntities(scene, entityID, ctx.Position, ctx.DetectRange);

    return ctx;
}

std::vector<NearbyEntity> AIManager::FindNearbyEntities(
    Scene& scene, u32 selfID, const glm::vec3& pos, f32 range) {
    
    std::vector<NearbyEntity> result;
    auto& world = scene.GetWorld();

    for (auto e : world.GetEntities()) {
        if (e == selfID) continue;

        auto* tr = world.GetComponent<TransformComponent>(e);
        if (!tr) continue;

        glm::vec3 ePos = {tr->X, tr->Y, tr->Z};
        f32 dist = glm::length(ePos - pos);

        if (dist > range) continue;

        NearbyEntity ne;
        ne.EntityID = e;
        ne.Position = ePos;
        ne.Distance = dist;

        if (auto* hp = world.GetComponent<HealthComponent>(e)) {
            ne.Health = hp->Current;
        }
        if (auto* tag = world.GetComponent<TagComponent>(e)) {
            ne.Tag = tag->Name;
        }

        result.push_back(ne);
    }

    // 按距离排序
    std::sort(result.begin(), result.end(),
        [](const NearbyEntity& a, const NearbyEntity& b) {
            return a.Distance < b.Distance;
        });

    return result;
}

void AIManager::ApplyAction(Scene& scene, u32 entityID, const AIAction& action, f32 dt) {
    auto& world = scene.GetWorld();
    auto* tr = world.GetComponent<TransformComponent>(entityID);
    if (!tr) return;

    // 应用移动
    if (action.MoveSpeed > 0.001f) {
        glm::vec3 dir = action.MoveDirection;
        f32 len = glm::length(dir);
        if (len > 0.001f) {
            dir /= len; // 归一化
            f32 speed = action.MoveSpeed * dt;
            tr->X += dir.x * speed;
            tr->Y += dir.y * speed;
            tr->Z += dir.z * speed;

            // 面向移动方向 (Y 轴旋转)
            tr->RotY = glm::degrees(atan2f(dir.x, dir.z));
        }
    }
}

AIAction AIManager::ParseAction(const std::string& result) {
    AIAction action;
    if (result.empty()) return action;

    // 格式: "state|dir_x,dir_y,dir_z|speed|target_id|custom"
    // 最简格式: "state" (向后兼容)
    std::vector<std::string> parts;
    std::istringstream ss(result);
    std::string part;
    while (std::getline(ss, part, '|')) {
        parts.push_back(part);
    }

    if (parts.empty()) return action;

    // [0] 新状态
    action.NewState = AIStateFromString(parts[0]);

    // [1] 移动方向 "x,y,z"
    if (parts.size() > 1 && !parts[1].empty()) {
        f32 x = 0, y = 0, z = 0;
        if (sscanf(parts[1].c_str(), "%f,%f,%f", &x, &y, &z) >= 2) {
            action.MoveDirection = {x, y, z};
        }
    }

    // [2] 移动速度
    if (parts.size() > 2 && !parts[2].empty()) {
        try { action.MoveSpeed = std::stof(parts[2]); }
        catch (...) { LOG_WARN("[AI] ParseAction: 无效速度 '%s'", parts[2].c_str()); }
    }

    // [3] 目标实体 ID
    if (parts.size() > 3 && !parts[3].empty()) {
        try { action.TargetEntityID = (u32)std::stoul(parts[3]); }
        catch (...) { LOG_WARN("[AI] ParseAction: 无效目标ID '%s'", parts[3].c_str()); }
    }

    // [4] 自定义动作
    if (parts.size() > 4) {
        action.CustomAction = parts[4];
    }

    return action;
}

std::vector<std::string> AIManager::ContextToArgs(const AIContext& ctx) {
    // 保留，未来可能用于其他传参方式
    return {};
}

} // namespace AI
} // namespace Engine

#else // !ENGINE_HAS_PYTHON

// ── 无 Python 时的 stub 实现 ────────────────────────────────

#include "engine/ai/python_engine.h"
#include "engine/core/scene.h"

namespace Engine {
namespace AI {

bool PythonEngine::s_Initialized = false;
std::string PythonEngine::s_LastError;

bool PythonEngine::Init(const std::string&) {
    LOG_WARN("[AI] Python 未启用 (编译时使用 -DENGINE_ENABLE_PYTHON=ON)");
    return false;
}
void PythonEngine::Shutdown() {}
bool PythonEngine::IsInitialized() { return false; }
bool PythonEngine::Execute(const std::string&) { return false; }
bool PythonEngine::ExecuteFile(const std::string&) { return false; }
std::string PythonEngine::CallFunction(const std::string&, const std::string&, const std::vector<std::string>&) { return ""; }
std::string PythonEngine::GetVariable(const std::string&, const std::string&) { return ""; }
std::string PythonEngine::GetLastError() { return "Python not enabled"; }

const char* AIStateToString(AIState state) {
    switch (state) {
        case AIState::Idle:   return "Idle";
        case AIState::Patrol: return "Patrol";
        case AIState::Chase:  return "Chase";
        case AIState::Attack: return "Attack";
        case AIState::Flee:   return "Flee";
        case AIState::Dead:   return "Dead";
        default:              return "Unknown";
    }
}

AIState AIStateFromString(const std::string& str) {
    if (str == "Patrol")  return AIState::Patrol;
    if (str == "Chase")   return AIState::Chase;
    if (str == "Attack")  return AIState::Attack;
    if (str == "Flee")    return AIState::Flee;
    if (str == "Dead")    return AIState::Dead;
    return AIState::Idle;
}

AIAction AIAgent::UpdateAI(const AIContext&) { return {}; }

u32 AIManager::s_AgentCount = 0;
void AIManager::Init() { LOG_WARN("[AI] AIManager: Python 未启用"); }
void AIManager::Shutdown() {}
void AIManager::Update(Scene&, f32) {}
AIContext AIManager::BuildContext(Scene&, u32, f32) { return {}; }
std::vector<NearbyEntity> AIManager::FindNearbyEntities(Scene&, u32, const glm::vec3&, f32) { return {}; }
void AIManager::ApplyAction(Scene&, u32, const AIAction&, f32) {}
std::vector<std::string> AIManager::ContextToArgs(const AIContext&) { return {}; }
AIAction AIManager::ParseAction(const std::string&) { return {}; }

} // namespace AI
} // namespace Engine

#endif // ENGINE_HAS_PYTHON
