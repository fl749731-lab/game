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
// PlayerTracker — 玩家行为追踪
// ════════════════════════════════════════════════════════════

std::deque<PlayerSnapshot> PlayerTracker::s_History;
u32 PlayerTracker::s_PlayerEntity = INVALID_ENTITY;
glm::vec3 PlayerTracker::s_LastPosition = {0,0,0};
f32 PlayerTracker::s_TotalTime = 0;
std::deque<f32> PlayerTracker::s_AttackTimes;
std::deque<f32> PlayerTracker::s_RetreatTimes;
f32 PlayerTracker::s_CombatTimer = 0;
bool PlayerTracker::s_InCombat = false;

void PlayerTracker::Update(Scene& scene, f32 dt) {
    s_TotalTime += dt;
    auto& world = scene.GetWorld();

    // 找到玩家实体（带 "Player" tag 的实体）
    u32 playerEntity = INVALID_ENTITY;
    for (auto e : world.GetEntities()) {
        auto* tag = world.GetComponent<TagComponent>(e);
        if (tag && (tag->Name == "Player" || tag->Name == "player")) {
            playerEntity = e;
            break;
        }
        // 或者看 SquadComponent 角色
        auto* sq = world.GetComponent<SquadComponent>(e);
        if (sq && sq->Role == "player") {
            playerEntity = e;
            break;
        }
    }

    if (playerEntity == INVALID_ENTITY) return;
    s_PlayerEntity = playerEntity;

    auto* tr = world.GetComponent<TransformComponent>(playerEntity);
    if (!tr) return;

    glm::vec3 pos = {tr->X, tr->Y, tr->Z};
    glm::vec3 vel = (s_TotalTime > dt) ? (pos - s_LastPosition) / dt : glm::vec3(0);
    f32 speed = glm::length(vel);

    // 检测后退（速度方向与朝敌方向相反）
    if (speed > 0.5f) {
        // 简化：如果远离最近的 AI 实体，算后退
        glm::vec3 toLastPos = pos - s_LastPosition;
        if (glm::length(toLastPos) > 0.1f) {
            // 此处仅记录，具体检测在 FindNearbyEntities 后做
        }
    }

    // 记录快照
    PlayerSnapshot snap;
    snap.Position = pos;
    snap.Velocity = vel;
    snap.Speed = speed;
    snap.Timestamp = s_TotalTime;
    s_History.push_back(snap);

    while (s_History.size() > MAX_HISTORY) s_History.pop_front();

    // 清理过期事件
    while (!s_AttackTimes.empty() && s_TotalTime - s_AttackTimes.front() > EVENT_WINDOW)
        s_AttackTimes.pop_front();
    while (!s_RetreatTimes.empty() && s_TotalTime - s_RetreatTimes.front() > EVENT_WINDOW)
        s_RetreatTimes.pop_front();

    // 战斗计时器
    if (s_InCombat) s_CombatTimer += dt;

    s_LastPosition = pos;
}

void PlayerTracker::Reset() {
    s_History.clear();
    s_AttackTimes.clear();
    s_RetreatTimes.clear();
    s_PlayerEntity = INVALID_ENTITY;
    s_LastPosition = {0,0,0};
    s_TotalTime = 0;
    s_CombatTimer = 0;
    s_InCombat = false;
}

glm::vec3 PlayerTracker::GetPlayerPosition() {
    return s_History.empty() ? glm::vec3(0) : s_History.back().Position;
}

glm::vec3 PlayerTracker::GetPlayerVelocity() {
    return s_History.empty() ? glm::vec3(0) : s_History.back().Velocity;
}

f32 PlayerTracker::GetPlayerSpeed() {
    return s_History.empty() ? 0 : s_History.back().Speed;
}

f32 PlayerTracker::GetAverageSpeed() {
    if (s_History.empty()) return 0;
    f32 total = 0;
    for (auto& s : s_History) total += s.Speed;
    return total / (f32)s_History.size();
}

const std::deque<PlayerSnapshot>& PlayerTracker::GetHistory() { return s_History; }
u32 PlayerTracker::GetAttackCount() { return (u32)s_AttackTimes.size(); }
u32 PlayerTracker::GetRetreatCount() { return (u32)s_RetreatTimes.size(); }

f32 PlayerTracker::GetAggressionScore() {
    u32 total = (u32)(s_AttackTimes.size() + s_RetreatTimes.size());
    if (total == 0) return 0.5f;
    return (f32)s_AttackTimes.size() / (f32)total;
}

f32 PlayerTracker::GetCombatTime() { return s_CombatTimer; }

void PlayerTracker::RecordAttack() {
    s_AttackTimes.push_back(s_TotalTime);
    s_InCombat = true;
}

void PlayerTracker::RecordRetreat() {
    s_RetreatTimes.push_back(s_TotalTime);
}

std::string PlayerTracker::ToJSON() {
    std::ostringstream ss;
    auto pos = GetPlayerPosition();
    auto vel = GetPlayerVelocity();
    ss << "{";
    ss << "\"pos\":[" << pos.x << "," << pos.y << "," << pos.z << "],";
    ss << "\"vel\":[" << vel.x << "," << vel.y << "," << vel.z << "],";
    ss << "\"speed\":" << GetPlayerSpeed() << ",";
    ss << "\"avg_speed\":" << GetAverageSpeed() << ",";
    ss << "\"attack_count\":" << GetAttackCount() << ",";
    ss << "\"retreat_count\":" << GetRetreatCount() << ",";
    ss << "\"aggression\":" << GetAggressionScore() << ",";
    ss << "\"combat_time\":" << GetCombatTime();
    ss << "}";
    return ss.str();
}

// ════════════════════════════════════════════════════════════
// AIAgent
// ════════════════════════════════════════════════════════════

AIAction AIAgent::UpdateAI(const AIContext& ctx) {
    AIAction action;
    action.NewState = ctx.CurrentState;

    if (!PythonEngine::IsInitialized()) return action;

    std::string ctxJson = AIManager::ContextToJSON(ctx);
    std::string result = PythonEngine::CallFunction(ScriptModule, "update_ai", {ctxJson});

    action = AIManager::ParseAction(result);
    return action;
}

// ════════════════════════════════════════════════════════════
// AIManager — 三阶段层级更新
// ════════════════════════════════════════════════════════════

u32 AIManager::s_AgentCount = 0;

void AIManager::Init() {
    s_AgentCount = 0;
    PlayerTracker::Reset();
    LOG_INFO("[AI] AIManager 已初始化 (层级指挥链模式)");
}

void AIManager::Shutdown() {
    s_AgentCount = 0;
    PlayerTracker::Reset();
    LOG_DEBUG("[AI] AIManager 已关闭");
}

void AIManager::Update(Scene& scene, f32 dt) {
    if (!PythonEngine::IsInitialized()) return;

    // 0. 更新玩家行为追踪
    PlayerTracker::Update(scene, dt);

    // 1. 指挥官决策（全局态势 → 下发战术命令给队长）
    UpdateCommanders(scene, dt);

    // 2. 小队长决策（接收命令 → 分解为子命令给士兵）
    UpdateSquadLeaders(scene, dt);

    // 3. 士兵执行（接收子命令 → 本地决策 → 行动）
    UpdateSoldiers(scene, dt);
}

// ── 阶段1：指挥官 ──────────────────────────────────────

void AIManager::UpdateCommanders(Scene& scene, f32 dt) {
    auto& world = scene.GetWorld();

    for (auto e : world.GetEntities()) {
        auto* sq = world.GetComponent<SquadComponent>(e);
        if (!sq || sq->Role != "commander") continue;

        auto* aiComp = world.GetComponent<AIComponent>(e);
        if (!aiComp) continue;

        auto* hpComp = world.GetComponent<HealthComponent>(e);
        if (hpComp && hpComp->Current <= 0) continue;

        AIContext ctx = BuildContext(scene, e, dt);
        ctx.Role = "commander";

        // 注入玩家行为数据
        InjectPlayerData(ctx);

        // 注入所有小队概览
        InjectCommanderData(scene, ctx);

        AIAgent agent;
        agent.EntityID = e;
        agent.State = AIStateFromString(aiComp->State);
        agent.DetectRange = aiComp->DetectRange;
        agent.AttackRange = aiComp->AttackRange;
        agent.ScriptModule = aiComp->ScriptModule;

        AIAction action = agent.UpdateAI(ctx);
        aiComp->State = AIStateToString(action.NewState);
        ApplyAction(scene, e, action, dt);

        // 下发命令给所属队长
        if (!action.OrderForSubordinates.empty()) {
            DispatchOrders(scene, e, action.OrderForSubordinates, "leader");
        }

        s_AgentCount++;
    }
}

// ── 阶段2：小队长 ──────────────────────────────────────

void AIManager::UpdateSquadLeaders(Scene& scene, f32 dt) {
    auto& world = scene.GetWorld();

    for (auto e : world.GetEntities()) {
        auto* sq = world.GetComponent<SquadComponent>(e);
        if (!sq || sq->Role != "leader") continue;

        auto* aiComp = world.GetComponent<AIComponent>(e);
        if (!aiComp) continue;

        auto* hpComp = world.GetComponent<HealthComponent>(e);
        if (hpComp && hpComp->Current <= 0) continue;

        AIContext ctx = BuildContext(scene, e, dt);
        ctx.Role = "leader";
        ctx.CurrentOrder = sq->CurrentOrder;
        ctx.SquadID = sq->SquadID;

        InjectPlayerData(ctx);
        InjectSquadData(scene, ctx, e);

        AIAgent agent;
        agent.EntityID = e;
        agent.State = AIStateFromString(aiComp->State);
        agent.DetectRange = aiComp->DetectRange;
        agent.AttackRange = aiComp->AttackRange;
        agent.ScriptModule = aiComp->ScriptModule;

        AIAction action = agent.UpdateAI(ctx);
        aiComp->State = AIStateToString(action.NewState);
        ApplyAction(scene, e, action, dt);

        // 下发子命令给本小队士兵
        if (!action.OrderForSubordinates.empty()) {
            DispatchOrders(scene, e, action.OrderForSubordinates, "soldier");
        }

        sq->OrderStatus = "executing";
        s_AgentCount++;
    }
}

// ── 阶段3：士兵 ────────────────────────────────────────

void AIManager::UpdateSoldiers(Scene& scene, f32 dt) {
    auto& world = scene.GetWorld();

    // AI 距离 LOD: 远处 AI 降低更新频率
    static u32 s_FrameCounter = 0;
    s_FrameCounter++;
    glm::vec3 playerPos = PlayerTracker::GetPlayerPosition();

    for (auto e : world.GetEntities()) {
        auto* sq = world.GetComponent<SquadComponent>(e);
        auto* aiComp = world.GetComponent<AIComponent>(e);
        if (!aiComp) continue;

        // 跳过指挥官和队长
        if (sq && (sq->Role == "commander" || sq->Role == "leader")) continue;

        auto* hpComp = world.GetComponent<HealthComponent>(e);
        if (hpComp && hpComp->Current <= 0) continue;

        // ── 距离 LOD ──────────────────────────────────────
        auto* tr = world.GetComponent<TransformComponent>(e);
        if (tr) {
            f32 dist = glm::distance(tr->GetWorldPosition(), playerPos);
            // 近距离 (<30): 每帧更新
            // 中距离 (30~60): 每 2 帧更新
            // 远距离 (>60): 每 4 帧更新
            if (dist > 60.0f && (s_FrameCounter % 4) != 0) continue;
            if (dist > 30.0f && (s_FrameCounter % 2) != 0) continue;
        }
        // ────────────────────────────────────────────────

        AIContext ctx = BuildContext(scene, e, dt);

        if (sq) {
            ctx.Role = "soldier";
            ctx.SquadID = sq->SquadID;
            ctx.CurrentOrder = sq->CurrentOrder;
            InjectSquadData(scene, ctx, e);
        }

        AIAgent agent;
        agent.EntityID = e;
        agent.State = AIStateFromString(aiComp->State);
        agent.DetectRange = aiComp->DetectRange;
        agent.AttackRange = aiComp->AttackRange;
        agent.ScriptModule = aiComp->ScriptModule;

        AIAction action = agent.UpdateAI(ctx);
        aiComp->State = AIStateToString(action.NewState);
        ApplyAction(scene, e, action, dt);

        if (sq) sq->OrderStatus = "executing";
        s_AgentCount++;
    }
}

// ── 命令下发 ────────────────────────────────────────────

void AIManager::DispatchOrders(Scene& scene, u32 issuerEntity,
                                const std::string& orderJson,
                                const std::string& targetRole) {
    auto& world = scene.GetWorld();
    auto* issuerSq = world.GetComponent<SquadComponent>(issuerEntity);
    if (!issuerSq) return;

    for (auto e : world.GetEntities()) {
        if (e == issuerEntity) continue;
        auto* sq = world.GetComponent<SquadComponent>(e);
        if (!sq) continue;

        bool shouldReceive = false;

        if (issuerSq->Role == "commander") {
            // 指挥官 → 队长：同一个指挥官下的队长
            if (targetRole == "leader" && sq->Role == "leader" && sq->CommanderEntity == issuerEntity)
                shouldReceive = true;
        } else if (issuerSq->Role == "leader") {
            // 队长 → 士兵：同一小队的士兵
            if (targetRole == "soldier" && sq->Role == "soldier" && sq->SquadID == issuerSq->SquadID)
                shouldReceive = true;
        }

        if (shouldReceive) {
            sq->CurrentOrder = orderJson;
            sq->OrderStatus = "pending";
        }
    }
}

// ── 上下文构建 ──────────────────────────────────────────

AIContext AIManager::BuildContext(Scene& scene, u32 entityID, f32 dt) {
    AIContext ctx;
    ctx.EntityID = entityID;
    ctx.DeltaTime = dt;

    auto& world = scene.GetWorld();

    if (auto* tr = world.GetComponent<TransformComponent>(entityID)) {
        ctx.Position = {tr->X, tr->Y, tr->Z};
        ctx.Rotation = {tr->RotX, tr->RotY, tr->RotZ};
    }

    if (auto* hp = world.GetComponent<HealthComponent>(entityID)) {
        ctx.Health = hp->Current;
        ctx.MaxHealth = hp->Max;
    }

    if (auto* ai = world.GetComponent<AIComponent>(entityID)) {
        ctx.CurrentState = AIStateFromString(ai->State);
        ctx.DetectRange = ai->DetectRange;
        ctx.AttackRange = ai->AttackRange;
    }

    ctx.NearbyEnemies = FindNearbyEntities(scene, entityID, ctx.Position, ctx.DetectRange);

    return ctx;
}

void AIManager::InjectPlayerData(AIContext& ctx) {
    ctx.HasPlayerData = true;
    ctx.PlayerPosition = PlayerTracker::GetPlayerPosition();
    ctx.PlayerVelocity = PlayerTracker::GetPlayerVelocity();
    ctx.PlayerSpeed = PlayerTracker::GetPlayerSpeed();
    ctx.PlayerAvgSpeed = PlayerTracker::GetAverageSpeed();
    ctx.PlayerAttackCount = PlayerTracker::GetAttackCount();
    ctx.PlayerRetreatCount = PlayerTracker::GetRetreatCount();
    ctx.PlayerAggressionScore = PlayerTracker::GetAggressionScore();
    ctx.PlayerCombatTime = PlayerTracker::GetCombatTime();
}

void AIManager::InjectSquadData(Scene& scene, AIContext& ctx, u32 entityID) {
    auto& world = scene.GetWorld();
    auto* mySq = world.GetComponent<SquadComponent>(entityID);
    if (!mySq) return;

    ctx.SquadID = mySq->SquadID;
    u32 total = 0, alive = 0;

    for (auto e : world.GetEntities()) {
        if (e == entityID) continue;
        auto* sq = world.GetComponent<SquadComponent>(e);
        if (!sq || sq->SquadID != mySq->SquadID) continue;

        total++;

        auto* tr = world.GetComponent<TransformComponent>(e);
        auto* hp = world.GetComponent<HealthComponent>(e);
        auto* ai = world.GetComponent<AIComponent>(e);

        if (hp && hp->Current <= 0) continue;
        alive++;

        AllyInfo ally;
        ally.EntityID = e;
        if (tr) {
            ally.Position = {tr->X, tr->Y, tr->Z};
            ally.Distance = glm::length(ally.Position - ctx.Position);
        }
        if (hp) { ally.Health = hp->Current; ally.MaxHealth = hp->Max; }
        if (ai) ally.State = ai->State;
        ally.Role = sq->Role;

        ctx.SquadMembers.push_back(ally);
    }

    ctx.SquadSize = total + 1;
    ctx.SquadAlive = alive + 1;
}

void AIManager::InjectCommanderData(Scene& scene, AIContext& ctx) {
    auto& world = scene.GetWorld();

    // 收集所有小队信息
    std::unordered_map<u32, AIContext::SquadSummary> squads;

    for (auto e : world.GetEntities()) {
        auto* sq = world.GetComponent<SquadComponent>(e);
        if (!sq || sq->SquadID == 0) continue;

        auto& summary = squads[sq->SquadID];
        summary.SquadID = sq->SquadID;
        summary.TotalMembers++;

        auto* hp = world.GetComponent<HealthComponent>(e);
        auto* tr = world.GetComponent<TransformComponent>(e);

        if (hp && hp->Current > 0) {
            summary.AliveMembers++;
            summary.AverageHealth += hp->Current;
        }
        if (tr) {
            summary.CenterPosition += glm::vec3(tr->X, tr->Y, tr->Z);
        }

        if (sq->Role == "leader") {
            summary.CurrentOrder = sq->CurrentOrder.empty() ? "idle" : "active";
        }
    }

    for (auto& [id, s] : squads) {
        if (s.TotalMembers > 0) {
            s.AverageHealth /= (f32)s.AliveMembers;
            s.CenterPosition /= (f32)s.TotalMembers;
        }
        ctx.AllSquads.push_back(s);
    }
}

// ── 附近实体查找 ────────────────────────────────────────

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

    std::sort(result.begin(), result.end(),
        [](const NearbyEntity& a, const NearbyEntity& b) {
            return a.Distance < b.Distance;
        });

    return result;
}

// ── 动作应用 ────────────────────────────────────────────

void AIManager::ApplyAction(Scene& scene, u32 entityID, const AIAction& action, f32 dt) {
    auto& world = scene.GetWorld();
    auto* tr = world.GetComponent<TransformComponent>(entityID);
    if (!tr) return;

    if (action.MoveSpeed > 0.001f) {
        glm::vec3 dir = action.MoveDirection;
        f32 len = glm::length(dir);
        if (len > 0.001f) {
            dir /= len;
            f32 speed = action.MoveSpeed * dt;
            tr->X += dir.x * speed;
            tr->Y += dir.y * speed;
            tr->Z += dir.z * speed;
            tr->RotY = glm::degrees(atan2f(dir.x, dir.z));
        }
    }
}

// ── JSON 序列化（完整版 — 含小队+玩家数据）───────────────

std::string AIManager::ContextToJSON(const AIContext& ctx) {
    std::ostringstream ss;
    ss << "{";

    // 基础信息
    ss << "\"entity_id\":" << ctx.EntityID << ",";
    ss << "\"pos\":[" << ctx.Position.x << "," << ctx.Position.y << "," << ctx.Position.z << "],";
    ss << "\"health\":" << ctx.Health << ",";
    ss << "\"max_health\":" << ctx.MaxHealth << ",";
    ss << "\"state\":\"" << AIStateToString(ctx.CurrentState) << "\",";
    ss << "\"detect_range\":" << ctx.DetectRange << ",";
    ss << "\"attack_range\":" << ctx.AttackRange << ",";
    ss << "\"move_speed\":" << ctx.MoveSpeed << ",";
    ss << "\"dt\":" << ctx.DeltaTime << ",";

    // 小队信息
    ss << "\"role\":\"" << ctx.Role << "\",";
    ss << "\"squad_id\":" << ctx.SquadID << ",";
    ss << "\"squad_size\":" << ctx.SquadSize << ",";
    ss << "\"squad_alive\":" << ctx.SquadAlive << ",";

    // 当前命令
    if (!ctx.CurrentOrder.empty()) {
        ss << "\"order\":" << ctx.CurrentOrder << ",";
    } else {
        ss << "\"order\":null,";
    }

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

    // 队友信息
    ss << "\"allies\":[";
    for (size_t i = 0; i < ctx.SquadMembers.size(); i++) {
        auto& a = ctx.SquadMembers[i];
        if (i > 0) ss << ",";
        ss << "{\"id\":" << a.EntityID
           << ",\"pos\":[" << a.Position.x << "," << a.Position.y << "," << a.Position.z << "]"
           << ",\"health\":" << a.Health
           << ",\"max_health\":" << a.MaxHealth
           << ",\"state\":\"" << a.State << "\""
           << ",\"role\":\"" << a.Role << "\""
           << ",\"dist\":" << a.Distance
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
    ss << "\"patrol_index\":" << ctx.CurrentPatrolIndex << ",";

    // 玩家行为数据（指挥官/队长可见）
    if (ctx.HasPlayerData) {
        ss << "\"player\":{";
        ss << "\"pos\":[" << ctx.PlayerPosition.x << "," << ctx.PlayerPosition.y << "," << ctx.PlayerPosition.z << "],";
        ss << "\"vel\":[" << ctx.PlayerVelocity.x << "," << ctx.PlayerVelocity.y << "," << ctx.PlayerVelocity.z << "],";
        ss << "\"speed\":" << ctx.PlayerSpeed << ",";
        ss << "\"avg_speed\":" << ctx.PlayerAvgSpeed << ",";
        ss << "\"attack_count\":" << ctx.PlayerAttackCount << ",";
        ss << "\"retreat_count\":" << ctx.PlayerRetreatCount << ",";
        ss << "\"aggression\":" << ctx.PlayerAggressionScore << ",";
        ss << "\"combat_time\":" << ctx.PlayerCombatTime;
        ss << "},";
    } else {
        ss << "\"player\":null,";
    }

    // 小队概览（指挥官可见）
    if (!ctx.AllSquads.empty()) {
        ss << "\"squads\":[";
        for (size_t i = 0; i < ctx.AllSquads.size(); i++) {
            auto& s = ctx.AllSquads[i];
            if (i > 0) ss << ",";
            ss << "{\"id\":" << s.SquadID
               << ",\"total\":" << s.TotalMembers
               << ",\"alive\":" << s.AliveMembers
               << ",\"avg_hp\":" << s.AverageHealth
               << ",\"center\":[" << s.CenterPosition.x << "," << s.CenterPosition.y << "," << s.CenterPosition.z << "]"
               << ",\"order\":\"" << s.CurrentOrder << "\""
               << "}";
        }
        ss << "]";
    } else {
        ss << "\"squads\":[]";
    }

    ss << "}";
    return ss.str();
}

// ── 返回值解析 ──────────────────────────────────────────

AIAction AIManager::ParseAction(const std::string& result) {
    AIAction action;
    if (result.empty()) return action;

    // 格式: "state|dir_x,dir_y,dir_z|speed|target_id|custom|order_json"
    std::vector<std::string> parts;
    std::istringstream ss(result);
    std::string part;
    while (std::getline(ss, part, '|')) {
        parts.push_back(part);
    }

    if (parts.empty()) return action;

    // [0] 新状态
    action.NewState = AIStateFromString(parts[0]);

    // [1] 移动方向
    if (parts.size() > 1 && !parts[1].empty()) {
        f32 x = 0, y = 0, z = 0;
        if (sscanf(parts[1].c_str(), "%f,%f,%f", &x, &y, &z) >= 2) {
            action.MoveDirection = {x, y, z};
        }
    }

    // [2] 速度
    if (parts.size() > 2 && !parts[2].empty()) {
        try { action.MoveSpeed = std::stof(parts[2]); }
        catch (...) {}
    }

    // [3] 目标
    if (parts.size() > 3 && !parts[3].empty()) {
        try { action.TargetEntityID = (u32)std::stoul(parts[3]); }
        catch (...) {}
    }

    // [4] 自定义动作
    if (parts.size() > 4) {
        action.CustomAction = parts[4];
    }

    // [5] 🆕 下发给下属的命令 JSON
    if (parts.size() > 5 && !parts[5].empty()) {
        action.OrderForSubordinates = parts[5];
    }

    return action;
}

std::vector<std::string> AIManager::ContextToArgs(const AIContext& ctx) {
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

// PlayerTracker stubs
std::deque<PlayerSnapshot> PlayerTracker::s_History;
u32 PlayerTracker::s_PlayerEntity = 0;
glm::vec3 PlayerTracker::s_LastPosition = {0,0,0};
f32 PlayerTracker::s_TotalTime = 0;
std::deque<f32> PlayerTracker::s_AttackTimes;
std::deque<f32> PlayerTracker::s_RetreatTimes;
f32 PlayerTracker::s_CombatTimer = 0;
bool PlayerTracker::s_InCombat = false;

void PlayerTracker::Update(Scene&, f32) {}
void PlayerTracker::Reset() {}
glm::vec3 PlayerTracker::GetPlayerPosition() { return {}; }
glm::vec3 PlayerTracker::GetPlayerVelocity() { return {}; }
f32 PlayerTracker::GetPlayerSpeed() { return 0; }
f32 PlayerTracker::GetAverageSpeed() { return 0; }
const std::deque<PlayerSnapshot>& PlayerTracker::GetHistory() { return s_History; }
u32 PlayerTracker::GetAttackCount() { return 0; }
u32 PlayerTracker::GetRetreatCount() { return 0; }
f32 PlayerTracker::GetAggressionScore() { return 0.5f; }
f32 PlayerTracker::GetCombatTime() { return 0; }
void PlayerTracker::RecordAttack() {}
void PlayerTracker::RecordRetreat() {}
std::string PlayerTracker::ToJSON() { return "{}"; }

u32 AIManager::s_AgentCount = 0;
void AIManager::Init() { LOG_WARN("[AI] AIManager: Python 未启用"); }
void AIManager::Shutdown() {}
void AIManager::Update(Scene&, f32) {}
void AIManager::UpdateCommanders(Scene&, f32) {}
void AIManager::UpdateSquadLeaders(Scene&, f32) {}
void AIManager::UpdateSoldiers(Scene&, f32) {}
AIContext AIManager::BuildContext(Scene&, u32, f32) { return {}; }
void AIManager::InjectPlayerData(AIContext&) {}
void AIManager::InjectSquadData(Scene&, AIContext&, u32) {}
void AIManager::InjectCommanderData(Scene&, AIContext&) {}
std::vector<NearbyEntity> AIManager::FindNearbyEntities(Scene&, u32, const glm::vec3&, f32) { return {}; }
void AIManager::ApplyAction(Scene&, u32, const AIAction&, f32) {}
void AIManager::DispatchOrders(Scene&, u32, const std::string&, const std::string&) {}
std::vector<std::string> AIManager::ContextToArgs(const AIContext&) { return {}; }
AIAction AIManager::ParseAction(const std::string&) { return {}; }
std::string AIManager::ContextToJSON(const AIContext&) { return "{}"; }

} // namespace AI
} // namespace Engine

#endif // ENGINE_HAS_PYTHON
