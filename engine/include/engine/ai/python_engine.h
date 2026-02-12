#pragma once

#include "engine/core/types.h"
#include "engine/core/log.h"
#include <string>
#include <vector>
#include <functional>

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

    /// 执行一段 Python 代码
    static bool Execute(const std::string& code);

    /// 执行 Python 脚本文件
    static bool ExecuteFile(const std::string& filepath);

    /// 调用 Python 函数 (模块名.函数名)，传入字典参数
    /// 返回函数的返回值（字符串形式）
    static std::string CallFunction(const std::string& module,
                                     const std::string& func,
                                     const std::vector<std::string>& args = {});

    /// 获取 Python 变量
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
};

// ── AI 动作 (Python 返回给 C++) ─────────────────────────────

struct AIAction {
    AIState NewState = AIState::Idle;
    glm::vec3 MoveDirection = {0, 0, 0};  // 归一化方向
    f32 MoveSpeed = 0;                     // 0 = 不移动
    u32 TargetEntityID = 0;                // 攻击目标
    std::string CustomAction = "";         // 自定义动作标签
};

// ── AI Agent ────────────────────────────────────────────────

struct AIAgent {
    u32 EntityID = 0;
    AIState State = AIState::Idle;
    f32 DetectRange = 10.0f;
    f32 AttackRange = 2.0f;
    f32 MoveSpeed = 3.0f;
    std::string ScriptModule = "default_ai";

    // 巡逻路径
    std::vector<glm::vec3> PatrolPoints;
    u32 CurrentPatrolIndex = 0;

    /// 调用 Python 脚本来决定下一步行为
    AIAction UpdateAI(const AIContext& ctx);
};

// ── AI 管理器 (批量管理所有 AI 实体) ────────────────────────

class AIManager {
public:
    /// 初始化
    static void Init();

    /// 每帧更新：遍历场景中所有 AI 实体
    static void Update(Scene& scene, f32 dt);

    /// 关闭
    static void Shutdown();

    /// 统计
    static u32 GetActiveAgentCount() { return s_AgentCount; }

private:
    /// 构造 AI 上下文
    static AIContext BuildContext(Scene& scene, u32 entityID, f32 dt);

    /// 查找附近敌人
    static std::vector<NearbyEntity> FindNearbyEntities(
        Scene& scene, u32 selfID, const glm::vec3& pos, f32 range);

    /// 应用 AI 动作到实体
    static void ApplyAction(Scene& scene, u32 entityID, const AIAction& action, f32 dt);

    /// 将上下文序列化为 Python 参数字符串
    static std::vector<std::string> ContextToArgs(const AIContext& ctx);

    /// 将 Python 返回值解析为 AIAction
    static AIAction ParseAction(const std::string& result);

    static u32 s_AgentCount;
};

} // namespace AI
} // namespace Engine
