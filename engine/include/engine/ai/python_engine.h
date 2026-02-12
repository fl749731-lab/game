#pragma once

#include "engine/core/types.h"
#include "engine/core/log.h"
#include <string>
#include <vector>
#include <functional>

namespace Engine {
namespace AI {

// ── Python AI 引擎 ──────────────────────────────────────────

class PythonEngine {
public:
    /// 初始化 Python 解释器
    static bool Init(const std::string& scriptsPath = "ai/scripts");
    /// 关闭 Python 解释器
    static void Shutdown();

    /// 判断是否已初始化
    static bool IsInitialized();

    /// 执行一段 Python 代码
    static bool Execute(const std::string& code);

    /// 执行 Python 脚本文件
    static bool ExecuteFile(const std::string& filepath);

    /// 调用 Python 函数 (模块名.函数名)
    /// 返回函数的返回值（字符串形式）
    static std::string CallFunction(const std::string& module, 
                                     const std::string& func,
                                     const std::vector<std::string>& args = {});

    /// 获取 Python 变量值（模块名.变量名）
    static std::string GetVariable(const std::string& module, 
                                    const std::string& varName);

    /// 获取最近的 Python 错误信息
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

// ── AI Agent (由 Python 脚本驱动决策) ───────────────────────

struct AIAgent {
    u32 EntityID = 0;
    AIState State = AIState::Idle;
    f32 Health = 100.0f;
    f32 DetectRange = 10.0f;
    f32 AttackRange = 2.0f;
    f32 MoveSpeed = 3.0f;
    std::string ScriptModule = "default_ai";

    /// 调用 Python 脚本来决定下一步行为
    void UpdateAI(f32 dt);
};

} // namespace AI
} // namespace Engine
