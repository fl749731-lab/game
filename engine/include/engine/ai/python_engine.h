#pragma once

#include "engine/core/types.h"
#include "engine/core/log.h"
#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <unordered_map>

#include <glm/glm.hpp>

namespace Engine {

// 前向声明
class Scene;

namespace AI {

// ── Python AI 引擎 ──────────────────────────────────────────

class PythonEngine {
public:
    static bool Init(const std::string& scriptsPath = "ai/scripts");
    static void Shutdown();
    static bool IsInitialized();

    static bool Execute(const std::string& code);
    static bool ExecuteFile(const std::string& filepath);

    static std::string CallFunction(const std::string& module,
                                     const std::string& func,
                                     const std::vector<std::string>& args = {});

    static std::string GetVariable(const std::string& module,
                                    const std::string& varName);

    static std::string GetLastError();

private:
    static bool s_Initialized;
    static std::string s_LastError;
};

// ── AI 行为状态 ─────────────────────────────────────────────

enum class AIState : u8 {
    Idle = 0,
    Patrol,
    Chase,
    Attack,
    Flee,
    Dead
};

const char* AIStateToString(AIState state);
AIState AIStateFromString(const std::string& str);

// ── 附近实体信息 ────────────────────────────────────────────

struct NearbyEntity {
    u32 EntityID = 0;
    glm::vec3 Position = {0, 0, 0};
    f32 Health = 0;
    f32 Distance = 0;
    std::string Tag = "";
};

// ── 玩家行为追踪器 ─────────────────────────────────────────

struct PlayerSnapshot {
    glm::vec3 Position = {0,0,0};
    glm::vec3 Velocity = {0,0,0};
    f32 Speed = 0;
    f32 Timestamp = 0;
};

class PlayerTracker {
public:
    /// 每帧更新玩家数据
    static void Update(Scene& scene, f32 dt);

    /// 重置所有追踪数据
    static void Reset();

    // ── 查询接口 ─────────────────────────────────
    static glm::vec3 GetPlayerPosition();
    static glm::vec3 GetPlayerVelocity();
    static f32 GetPlayerSpeed();
    static f32 GetAverageSpeed();

    /// 最近 N 秒的位置历史
    static const std::deque<PlayerSnapshot>& GetHistory();

    /// 攻击统计
    static u32 GetAttackCount();       // 近 N 秒攻击次数
    static u32 GetRetreatCount();      // 近 N 秒后退次数
    static f32 GetAggressionScore();   // 攻击倾向 0~1
    static f32 GetCombatTime();        // 战斗持续时间

    /// 记录玩家攻击事件（由外部调用）
    static void RecordAttack();
    /// 记录玩家后退事件
    static void RecordRetreat();

    /// 序列化为 JSON 字符串（传给 Python）
    static std::string ToJSON();

private:
    static std::deque<PlayerSnapshot> s_History;
    static u32 s_PlayerEntity;
    static glm::vec3 s_LastPosition;
    static f32 s_TotalTime;

    // 攻击/后退事件时间戳
    static std::deque<f32> s_AttackTimes;
    static std::deque<f32> s_RetreatTimes;
    static f32 s_CombatTimer;
    static bool s_InCombat;

    static constexpr u32 MAX_HISTORY = 300;    // 5秒 @ 60fps
    static constexpr f32 EVENT_WINDOW = 10.0f; // 10秒统计窗口
};

// ── 小队命令（指挥官/队长 下发）─────────────────────────────

struct SquadOrder {
    std::string Type = "idle";    // "attack"|"defend"|"flank_left"|"flank_right"|"retreat"|"regroup"|"hold"
    glm::vec3 TargetPos = {0,0,0};
    u32 TargetEntityID = 0;
    f32 Priority = 0.5f;         // 0~1 优先级
    std::string Formation = "triangle"; // "triangle"|"line"|"spread"|"wedge"
    std::string Extra;           // 额外 JSON 数据
};

// ── 友军信息（传给 AI 的队友数据）──────────────────────────

struct AllyInfo {
    u32 EntityID = 0;
    glm::vec3 Position = {0,0,0};
    f32 Health = 0;
    f32 MaxHealth = 0;
    std::string State = "Idle";
    std::string Role = "soldier";
    f32 Distance = 0;
};

// ── AI 上下文 (传给 Python 的完整信息) ──────────────────────

struct AIContext {
    // 自身信息
    u32 EntityID = 0;
    glm::vec3 Position = {0, 0, 0};
    glm::vec3 Rotation = {0, 0, 0};
    f32 Health = 100.0f;
    f32 MaxHealth = 100.0f;
    f32 DetectRange = 10.0f;
    f32 AttackRange = 2.0f;
    f32 MoveSpeed = 3.0f;
    AIState CurrentState = AIState::Idle;

    // 环境信息
    std::vector<NearbyEntity> NearbyEnemies;
    f32 DeltaTime = 0;

    // 巡逻路径点 (可选)
    std::vector<glm::vec3> PatrolPoints;
    u32 CurrentPatrolIndex = 0;

    // ── 🆕 小队信息 ──────────────────────────────
    u32 SquadID = 0;
    std::string Role = "soldier";       // "commander"|"leader"|"soldier"
    std::string CurrentOrder;           // 当前收到的命令 JSON
    std::vector<AllyInfo> SquadMembers; // 同小队队友信息
    u32 SquadSize = 0;
    u32 SquadAlive = 0;

    // ── 🆕 玩家行为数据（只有 commander/leader 收到）────
    bool HasPlayerData = false;
    glm::vec3 PlayerPosition = {0,0,0};
    glm::vec3 PlayerVelocity = {0,0,0};
    f32 PlayerSpeed = 0;
    f32 PlayerAvgSpeed = 0;
    u32 PlayerAttackCount = 0;
    u32 PlayerRetreatCount = 0;
    f32 PlayerAggressionScore = 0;
    f32 PlayerCombatTime = 0;

    // ── 🆕 小队状态概览（只有 commander 收到）───────────
    struct SquadSummary {
        u32 SquadID = 0;
        u32 TotalMembers = 0;
        u32 AliveMembers = 0;
        f32 AverageHealth = 0;
        glm::vec3 CenterPosition = {0,0,0};
        std::string CurrentOrder = "idle";
    };
    std::vector<SquadSummary> AllSquads;  // 指挥官可见所有小队
};

// ── AI 动作 (Python 返回给 C++) ─────────────────────────────

struct AIAction {
    AIState NewState = AIState::Idle;
    glm::vec3 MoveDirection = {0, 0, 0};
    f32 MoveSpeed = 0;
    u32 TargetEntityID = 0;
    std::string CustomAction = "";

    // 🆕 指挥官/队长下发的命令（写入下属的 SquadComponent::CurrentOrder）
    std::string OrderForSubordinates;
};

// ── AI Agent ────────────────────────────────────────────────

struct AIAgent {
    u32 EntityID = 0;
    AIState State = AIState::Idle;
    f32 DetectRange = 10.0f;
    f32 AttackRange = 2.0f;
    f32 MoveSpeed = 3.0f;
    std::string ScriptModule = "default_ai";

    std::vector<glm::vec3> PatrolPoints;
    u32 CurrentPatrolIndex = 0;

    AIAction UpdateAI(const AIContext& ctx);
};

// ── AI 管理器 ───────────────────────────────────────────────

class AIManager {
public:
    static void Init();
    static void Update(Scene& scene, f32 dt);
    static void Shutdown();
    static u32 GetActiveAgentCount() { return s_AgentCount; }

private:
    // 三阶段更新
    static void UpdateCommanders(Scene& scene, f32 dt);
    static void UpdateSquadLeaders(Scene& scene, f32 dt);
    static void UpdateSoldiers(Scene& scene, f32 dt);

    static AIContext BuildContext(Scene& scene, u32 entityID, f32 dt);
    static void InjectPlayerData(AIContext& ctx);
    static void InjectSquadData(Scene& scene, AIContext& ctx, u32 entityID);
    static void InjectCommanderData(Scene& scene, AIContext& ctx);

    static std::vector<NearbyEntity> FindNearbyEntities(
        Scene& scene, u32 selfID, const glm::vec3& pos, f32 range);

    static void ApplyAction(Scene& scene, u32 entityID, const AIAction& action, f32 dt);
    static void DispatchOrders(Scene& scene, u32 issuerEntity,
                                const std::string& orderJson, const std::string& role);

    static std::vector<std::string> ContextToArgs(const AIContext& ctx);
    static AIAction ParseAction(const std::string& result);

    /// 序列化上下文为 JSON（完整版，含小队+玩家数据）
    static std::string ContextToJSON(const AIContext& ctx);

    static u32 s_AgentCount;
};

} // namespace AI
} // namespace Engine
