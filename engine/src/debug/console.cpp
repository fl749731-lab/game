#include "engine/debug/console.h"
#include "engine/debug/stat_system.h"
#include "engine/core/log.h"

#include <cstdarg>
#include <cstring>
#include <algorithm>
#include <sstream>

namespace Engine {

void Console::Init() {
    s_Visible = false;
    s_LogEntries.clear();
    s_CommandHistory.clear();
    s_CVars.clear();
    s_Commands.clear();
    s_HistoryPos = -1;
    memset(s_InputBuffer, 0, sizeof(s_InputBuffer));

    RegisterBuiltinCommands();

    // 注册 Logger 回调 — 引擎所有 LOG_INFO/WARN/ERROR 自动输出到控制台
    Logger::SetCallback([](Engine::LogLevel level, const char* message) {
        Console::LogLevel consoleLevel = Console::LogLevel::Info;
        if (level == Engine::LogLevel::Warn)  consoleLevel = Console::LogLevel::Warning;
        if (level == Engine::LogLevel::Error || level == Engine::LogLevel::Fatal)
            consoleLevel = Console::LogLevel::Error;
        Console::Log(message, consoleLevel);
    });

    Log("引擎控制台 v1.0 — 输入 'help' 查看命令列表", LogLevel::Info);
    LOG_INFO("[Console] 初始化");
}

void Console::Shutdown() {
    s_LogEntries.clear();
    s_CVars.clear();
    s_Commands.clear();
    LOG_INFO("[Console] 关闭");
}

void Console::Toggle() { s_Visible = !s_Visible; if (s_Visible) s_FocusInput = true; }
bool Console::IsVisible() { return s_Visible; }
void Console::SetVisible(bool v) { s_Visible = v; if (v) s_FocusInput = true; }

// ── CVar 注册 ───────────────────────────────────────────────

void Console::RegisterCVar(const std::string& name, i32 val, const std::string& desc, CVarFlags flags) {
    CVarEntry e; e.Name = name; e.Description = desc; e.Flags = flags;
    e.Value = val; e.DefaultValue = val;
    s_CVars[name] = e;
}
void Console::RegisterCVar(const std::string& name, f32 val, const std::string& desc, CVarFlags flags) {
    CVarEntry e; e.Name = name; e.Description = desc; e.Flags = flags;
    e.Value = val; e.DefaultValue = val;
    s_CVars[name] = e;
}
void Console::RegisterCVar(const std::string& name, bool val, const std::string& desc, CVarFlags flags) {
    CVarEntry e; e.Name = name; e.Description = desc; e.Flags = flags;
    e.Value = val; e.DefaultValue = val;
    s_CVars[name] = e;
}
void Console::RegisterCVar(const std::string& name, const std::string& val, const std::string& desc, CVarFlags flags) {
    CVarEntry e; e.Name = name; e.Description = desc; e.Flags = flags;
    e.Value = val; e.DefaultValue = val;
    s_CVars[name] = e;
}

CVarEntry* Console::FindCVar(const std::string& name) {
    auto it = s_CVars.find(name);
    return (it != s_CVars.end()) ? &it->second : nullptr;
}

i32 Console::GetCVarInt(const std::string& name, i32 fb) {
    auto* cv = FindCVar(name); return cv ? cv->AsInt() : fb;
}
f32 Console::GetCVarFloat(const std::string& name, f32 fb) {
    auto* cv = FindCVar(name); return cv ? cv->AsFloat() : fb;
}
bool Console::GetCVarBool(const std::string& name, bool fb) {
    auto* cv = FindCVar(name); return cv ? cv->AsBool() : fb;
}

void Console::SetCVar(const std::string& name, i32 value) {
    auto* cv = FindCVar(name);
    if (cv && !((u8)cv->Flags & (u8)CVarFlags::ReadOnly)) cv->Value = value;
}
void Console::SetCVar(const std::string& name, f32 value) {
    auto* cv = FindCVar(name);
    if (cv && !((u8)cv->Flags & (u8)CVarFlags::ReadOnly)) cv->Value = value;
}
void Console::SetCVar(const std::string& name, bool value) {
    auto* cv = FindCVar(name);
    if (cv && !((u8)cv->Flags & (u8)CVarFlags::ReadOnly)) cv->Value = value;
}
void Console::SetCVar(const std::string& name, const std::string& value) {
    auto* cv = FindCVar(name);
    if (cv && !((u8)cv->Flags & (u8)CVarFlags::ReadOnly)) cv->Value = value;
}

// ── 命令注册 ────────────────────────────────────────────────

void Console::RegisterCommand(const std::string& name, CommandCallback callback,
                               const std::string& help) {
    s_Commands.push_back({name, help, callback});
}

void Console::Execute(const std::string& commandLine) {
    if (commandLine.empty()) return;

    // 日志记录
    Log("> " + commandLine, LogLevel::Command);

    // 历史记录
    s_CommandHistory.push_back(commandLine);
    if (s_CommandHistory.size() > MAX_HISTORY)
        s_CommandHistory.erase(s_CommandHistory.begin());
    s_HistoryPos = -1;

    auto tokens = Tokenize(commandLine);
    if (tokens.empty()) return;

    std::string cmd = tokens[0];
    std::vector<std::string> args(tokens.begin() + 1, tokens.end());
    ExecuteInternal(cmd, args);
}

void Console::ExecuteInternal(const std::string& cmd, const std::vector<std::string>& args) {
    // 查找命令
    for (auto& entry : s_Commands) {
        if (entry.Name == cmd) {
            entry.Callback(args);
            return;
        }
    }

    // 尝试作为 CVar 处理
    auto* cvar = FindCVar(cmd);
    if (cvar) {
        if (args.empty()) {
            // 显示当前值
            if (std::holds_alternative<i32>(cvar->Value))
                LogFmt(LogLevel::Info, "%s = %d  (%s)", cmd.c_str(), std::get<i32>(cvar->Value), cvar->Description.c_str());
            else if (std::holds_alternative<f32>(cvar->Value))
                LogFmt(LogLevel::Info, "%s = %.3f  (%s)", cmd.c_str(), std::get<f32>(cvar->Value), cvar->Description.c_str());
            else if (std::holds_alternative<bool>(cvar->Value))
                LogFmt(LogLevel::Info, "%s = %s  (%s)", cmd.c_str(), std::get<bool>(cvar->Value) ? "true" : "false", cvar->Description.c_str());
            else
                LogFmt(LogLevel::Info, "%s = %s  (%s)", cmd.c_str(), std::get<std::string>(cvar->Value).c_str(), cvar->Description.c_str());
        } else {
            // 设置值
            if ((u8)cvar->Flags & (u8)CVarFlags::ReadOnly) {
                LogFmt(LogLevel::Error, "%s 是只读变量", cmd.c_str());
                return;
            }
            if (std::holds_alternative<i32>(cvar->Value))
                cvar->Value = std::stoi(args[0]);
            else if (std::holds_alternative<f32>(cvar->Value))
                cvar->Value = std::stof(args[0]);
            else if (std::holds_alternative<bool>(cvar->Value))
                cvar->Value = (args[0] == "1" || args[0] == "true");
            else
                cvar->Value = args[0];

            LogFmt(LogLevel::Info, "%s 设置为 %s", cmd.c_str(), args[0].c_str());
        }
        return;
    }

    LogFmt(LogLevel::Error, "未知命令: '%s'", cmd.c_str());
}

// ── 日志 ────────────────────────────────────────────────────

void Console::Log(const std::string& message, LogLevel level) {
    s_LogEntries.push_back({message, level});
    if (s_LogEntries.size() > MAX_LOG_ENTRIES)
        s_LogEntries.pop_front();
}

void Console::LogFmt(LogLevel level, const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Log(buf, level);
}

// ── 解析 + 补全 ─────────────────────────────────────────────

std::vector<std::string> Console::Tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream ss(line);
    std::string token;
    while (ss >> token) tokens.push_back(token);
    return tokens;
}

std::vector<std::string> Console::GetAutocompleteSuggestions(const std::string& partial) {
    std::vector<std::string> results;
    if (partial.empty()) return results;

    // 命令
    for (auto& cmd : s_Commands) {
        if (cmd.Name.rfind(partial, 0) == 0)
            results.push_back(cmd.Name);
    }
    // CVar
    for (auto& [name, _] : s_CVars) {
        if (name.rfind(partial, 0) == 0)
            results.push_back(name);
    }

    std::sort(results.begin(), results.end());
    return results;
}

// ── 内置命令 ────────────────────────────────────────────────

void Console::RegisterBuiltinCommands() {
    RegisterCommand("help", [](const std::vector<std::string>&) {
        Console::Log("=== 命令列表 ===", LogLevel::Info);
        for (auto& cmd : s_Commands) {
            Console::LogFmt(LogLevel::Info, "  %-20s %s", cmd.Name.c_str(), cmd.Help.c_str());
        }
        Console::Log("=== CVar 列表 ===", LogLevel::Info);
        for (auto& [name, cvar] : s_CVars) {
            Console::LogFmt(LogLevel::Info, "  %-20s %s", name.c_str(), cvar.Description.c_str());
        }
    }, "显示所有命令和变量");

    RegisterCommand("clear", [](const std::vector<std::string>&) {
        s_LogEntries.clear();
    }, "清空控制台");

    RegisterCommand("stat", [](const std::vector<std::string>& args) {
        if (args.empty()) {
            Console::Log("用法: stat [fps|unit|gpu|memory|rendering|physics|scene|all]", LogLevel::Warning);
            return;
        }
        StatOverlay::ToggleByName(args[0]);
        Console::LogFmt(LogLevel::Info, "stat %s 已切换", args[0].c_str());
    }, "切换统计覆盖层");

    RegisterCommand("echo", [](const std::vector<std::string>& args) {
        std::string msg;
        for (auto& a : args) { msg += a + " "; }
        Console::Log(msg, LogLevel::Info);
    }, "输出消息");

    RegisterCommand("version", [](const std::vector<std::string>&) {
        Console::Log("Engine v0.1.0 | OpenGL 4.5 | C++20", LogLevel::Info);
    }, "显示引擎版本");

    // CVar: 渲染相关
    RegisterCVar("r.wireframe", false, "线框模式");
    RegisterCVar("r.shadowQuality", (i32)2, "阴影质量 0-4");
    RegisterCVar("r.exposure", 1.2f, "HDR 曝光度");
    RegisterCVar("r.bloom", true, "泛光效果");
    RegisterCVar("r.gbufDebug", (i32)0, "G-Buffer 调试模式");
    RegisterCVar("r.vsync", true, "垂直同步");
    RegisterCVar("r.fov", 60.0f, "视场角");
    RegisterCVar("debug.drawLines", true, "调试线框");
    RegisterCVar("debug.showBVH", false, "显示 BVH");
    RegisterCVar("audio.masterVolume", 1.0f, "主音量");
}

// ── ImGui 渲染 ──────────────────────────────────────────────

void Console::Render() {
    if (!s_Visible) return;

    ImGuiIO& io = ImGui::GetIO();
    float consoleHeight = io.DisplaySize.y * 0.4f;

    // 半透明深色背景 (屏幕顶部下拉)
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, consoleHeight));
    ImGui::SetNextWindowBgAlpha(0.88f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                              ImGuiWindowFlags_NoResize |
                              ImGuiWindowFlags_NoMove |
                              ImGuiWindowFlags_NoCollapse |
                              ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.08f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.5f, 0.8f, 0.5f));

    if (!ImGui::Begin("##Console", nullptr, flags)) {
        ImGui::PopStyleColor(2);
        ImGui::End();
        return;
    }

    // 标题
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "ENGINE CONSOLE");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
    ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1), "~ 关闭");
    ImGui::Separator();

    // 日志区域 (滚动)
    float logHeight = consoleHeight - 80.0f;
    ImGui::BeginChild("ConsoleLog", ImVec2(0, logHeight), false);

    for (auto& entry : s_LogEntries) {
        ImVec4 color;
        switch (entry.Level) {
            case LogLevel::Info:    color = ImVec4(0.8f, 0.8f, 0.8f, 1); break;
            case LogLevel::Warning: color = ImVec4(1.0f, 0.9f, 0.3f, 1); break;
            case LogLevel::Error:   color = ImVec4(1.0f, 0.3f, 0.3f, 1); break;
            case LogLevel::Command: color = ImVec4(0.3f, 0.9f, 1.0f, 1); break;
        }
        ImGui::TextColored(color, "%s", entry.Text.c_str());
    }

    // 自动滚动到底部
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();

    // 输入栏
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.15f, 1));

    ImGui::Text(">");
    ImGui::SameLine();

    ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue |
                                      ImGuiInputTextFlags_CallbackHistory |
                                      ImGuiInputTextFlags_CallbackCompletion;

    // 回调处理历史和补全
    auto textCallback = [](ImGuiInputTextCallbackData* data) -> int {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
            // Tab 补全
            std::string partial(data->Buf, data->CursorPos);
            auto suggestions = GetAutocompleteSuggestions(partial);
            if (suggestions.size() == 1) {
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, suggestions[0].c_str());
            } else if (suggestions.size() > 1) {
                std::string msg = "可选: ";
                for (auto& s : suggestions) msg += s + "  ";
                Console::Log(msg, LogLevel::Info);
            }
        }
        else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
            if (s_CommandHistory.empty()) return 0;

            if (data->EventKey == ImGuiKey_UpArrow) {
                if (s_HistoryPos < 0) s_HistoryPos = (i32)s_CommandHistory.size() - 1;
                else if (s_HistoryPos > 0) s_HistoryPos--;
            }
            else if (data->EventKey == ImGuiKey_DownArrow) {
                if (s_HistoryPos >= 0) s_HistoryPos++;
                if (s_HistoryPos >= (i32)s_CommandHistory.size()) s_HistoryPos = -1;
            }

            data->DeleteChars(0, data->BufTextLen);
            if (s_HistoryPos >= 0 && s_HistoryPos < (i32)s_CommandHistory.size())
                data->InsertChars(0, s_CommandHistory[s_HistoryPos].c_str());
        }
        return 0;
    };

    bool entered = ImGui::InputText("##ConsoleInput", s_InputBuffer, sizeof(s_InputBuffer),
                                     inputFlags, textCallback);

    if (s_FocusInput) {
        ImGui::SetKeyboardFocusHere(-1);
        s_FocusInput = false;
    }

    if (entered && s_InputBuffer[0] != '\0') {
        Execute(s_InputBuffer);
        memset(s_InputBuffer, 0, sizeof(s_InputBuffer));
        s_FocusInput = true;  // 重新聚焦
    }

    ImGui::PopStyleColor();  // FrameBg
    ImGui::End();
    ImGui::PopStyleColor(2); // WindowBg + Border
}

bool Console::HandleKeyInput(int key, int action) {
    // ` 或 ~ 键切换 (GLFW_KEY_GRAVE_ACCENT = 96)
    if (key == 96 && action == 1) {
        Toggle();
        return true;  // 消费事件
    }
    return false;
}

} // namespace Engine
