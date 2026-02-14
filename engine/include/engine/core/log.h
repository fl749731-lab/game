#pragma once

#include <string>
#include <cstdio>
#include <cstdarg>

namespace Engine {

// ── 日志级别 ────────────────────────────────────────────────

enum class LogLevel {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

// ── 日志回调 (Console 集成) ──────────────────────────────────

using LogCallback = void(*)(LogLevel level, const char* message);

// ── 日志系统 ────────────────────────────────────────────────

class Logger {
public:
    static void Init();
    static void SetLevel(LogLevel level);
    static void Log(LogLevel level, const char* file, int line, const char* fmt, ...);

    /// 设置日志回调 (Console 用来接收日志)
    static void SetCallback(LogCallback callback);

private:
    static LogLevel s_Level;
    static LogCallback s_Callback;
    static void SetConsoleColor(LogLevel level);
    static void ResetConsoleColor();
    static const char* LevelToString(LogLevel level);
};

} // namespace Engine

// ── 日志宏 ──────────────────────────────────────────────────

#define LOG_TRACE(fmt, ...) ::Engine::Logger::Log(::Engine::LogLevel::Trace, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) ::Engine::Logger::Log(::Engine::LogLevel::Debug, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  ::Engine::Logger::Log(::Engine::LogLevel::Info,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  ::Engine::Logger::Log(::Engine::LogLevel::Warn,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) ::Engine::Logger::Log(::Engine::LogLevel::Error, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) ::Engine::Logger::Log(::Engine::LogLevel::Fatal, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
