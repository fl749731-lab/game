#pragma once

#include "engine/core/types.h"

#include <imgui.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <variant>
#include <deque>

namespace Engine {

// ── CVar 标志位 ─────────────────────────────────────────────

enum class CVarFlags : u8 {
    None       = 0,
    ReadOnly   = 1 << 0,
    Cheat      = 1 << 1,
    SaveConfig = 1 << 2,
};

inline CVarFlags operator|(CVarFlags a, CVarFlags b) { return (CVarFlags)((u8)a|(u8)b); }
inline CVarFlags operator&(CVarFlags a, CVarFlags b) { return (CVarFlags)((u8)a&(u8)b); }

// ── CVar (控制台变量) ───────────────────────────────────────

struct CVarEntry {
    std::string Name;
    std::string Description;
    CVarFlags Flags = CVarFlags::None;
    std::variant<i32, f32, bool, std::string> Value;
    std::variant<i32, f32, bool, std::string> DefaultValue;

    // 取值
    i32 AsInt() const { return std::holds_alternative<i32>(Value) ? std::get<i32>(Value) : 0; }
    f32 AsFloat() const { return std::holds_alternative<f32>(Value) ? std::get<f32>(Value) : 0; }
    bool AsBool() const { return std::holds_alternative<bool>(Value) ? std::get<bool>(Value) : false; }
    const std::string& AsString() const {
        static std::string empty;
        return std::holds_alternative<std::string>(Value) ? std::get<std::string>(Value) : empty;
    }
};

// ── Console (引擎控制台终端) ────────────────────────────────
// 功能:
//   1. ~ 键切换下拉终端
//   2. CVar 系统 (int/float/bool/string)
//   3. 命令注册API + 内置命令
//   4. Tab 自动补全 + 历史 (↑↓)
//   5. 日志输出滚动
//   6. 颜色分级: 白=INFO, 黄=WARN, 红=ERROR, 青=CMD

class Console {
public:
    static void Init();
    static void Shutdown();

    /// 切换显示
    static void Toggle();
    static bool IsVisible();
    static void SetVisible(bool v);

    /// ImGui 渲染
    static void Render();

    /// 处理快捷键 (在 Application 主循环调用)
    static bool HandleKeyInput(int key, int action);

    // ── CVar API ────────────────────────────────────
    static void RegisterCVar(const std::string& name, i32 defaultVal,
                              const std::string& desc = "", CVarFlags flags = CVarFlags::None);
    static void RegisterCVar(const std::string& name, f32 defaultVal,
                              const std::string& desc = "", CVarFlags flags = CVarFlags::None);
    static void RegisterCVar(const std::string& name, bool defaultVal,
                              const std::string& desc = "", CVarFlags flags = CVarFlags::None);
    static void RegisterCVar(const std::string& name, const std::string& defaultVal,
                              const std::string& desc = "", CVarFlags flags = CVarFlags::None);

    static CVarEntry* FindCVar(const std::string& name);
    static i32 GetCVarInt(const std::string& name, i32 fallback = 0);
    static f32 GetCVarFloat(const std::string& name, f32 fallback = 0);
    static bool GetCVarBool(const std::string& name, bool fallback = false);

    static void SetCVar(const std::string& name, i32 value);
    static void SetCVar(const std::string& name, f32 value);
    static void SetCVar(const std::string& name, bool value);
    static void SetCVar(const std::string& name, const std::string& value);

    // ── 命令 API ────────────────────────────────────
    using CommandCallback = std::function<void(const std::vector<std::string>&)>;
    static void RegisterCommand(const std::string& name, CommandCallback callback,
                                 const std::string& help = "");

    /// 执行命令字符串
    static void Execute(const std::string& commandLine);

    // ── 日志 API ────────────────────────────────────
    enum class LogLevel : u8 { Info, Warning, Error, Command };

    static void Log(const std::string& message, LogLevel level = LogLevel::Info);
    static void LogFmt(LogLevel level, const char* fmt, ...);

private:
    static void ExecuteInternal(const std::string& cmd, const std::vector<std::string>& args);
    static std::vector<std::string> Tokenize(const std::string& line);
    static std::vector<std::string> GetAutocompleteSuggestions(const std::string& partial);

    // 内置命令注册
    static void RegisterBuiltinCommands();

    // ── 数据 ────────────────────────────────────────
    struct LogEntry {
        std::string Text;
        LogLevel Level;
    };

    struct CommandEntry {
        std::string Name;
        std::string Help;
        CommandCallback Callback;
    };

    inline static bool s_Visible = false;
    inline static bool s_FocusInput = false;
    inline static char s_InputBuffer[512] = {};

    // 日志
    inline static std::deque<LogEntry> s_LogEntries;
    static constexpr u32 MAX_LOG_ENTRIES = 500;

    // 命令历史
    inline static std::vector<std::string> s_CommandHistory;
    inline static i32 s_HistoryPos = -1;
    static constexpr u32 MAX_HISTORY = 50;

    // 注册表
    inline static std::unordered_map<std::string, CVarEntry> s_CVars;
    inline static std::vector<CommandEntry> s_Commands;

    // 补全
    inline static std::vector<std::string> s_Suggestions;
    inline static i32 s_SuggestionIdx = -1;
};

// ── CVar 注册宏 ─────────────────────────────────────────────

#define CVAR_INT(name, val, desc)    \
    static struct _CVar_##__LINE__ { \
        _CVar_##__LINE__() { Console::RegisterCVar(name, (i32)(val), desc); } \
    } _cvar_inst_##__LINE__;

#define CVAR_FLOAT(name, val, desc)  \
    static struct _CVar_##__LINE__ { \
        _CVar_##__LINE__() { Console::RegisterCVar(name, (f32)(val), desc); } \
    } _cvar_inst_##__LINE__;

#define CVAR_BOOL(name, val, desc)   \
    static struct _CVar_##__LINE__ { \
        _CVar_##__LINE__() { Console::RegisterCVar(name, (bool)(val), desc); } \
    } _cvar_inst_##__LINE__;

} // namespace Engine
