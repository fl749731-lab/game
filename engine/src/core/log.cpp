#include "engine/core/log.h"

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <mutex>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

namespace Engine {

LogLevel Logger::s_Level = LogLevel::Trace;
LogCallback Logger::s_Callback = nullptr;
static std::mutex s_LogMutex;

void Logger::Init() {
#ifdef _WIN32
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hConsole, &mode);
    SetConsoleMode(hConsole, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
    LOG_INFO("日志系统初始化完成");
}

void Logger::SetLevel(LogLevel level) {
    s_Level = level;
}

const char* Logger::LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
        default:              return "?????";
    }
}

void Logger::SetConsoleColor(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: printf("\033[90m");    break;
        case LogLevel::Debug: printf("\033[36m");    break;
        case LogLevel::Info:  printf("\033[32m");    break;
        case LogLevel::Warn:  printf("\033[33m");    break;
        case LogLevel::Error: printf("\033[31m");    break;
        case LogLevel::Fatal: printf("\033[1;31m");  break;
    }
}

void Logger::ResetConsoleColor() {
    printf("\033[0m");
}

void Logger::SetCallback(LogCallback callback) {
    s_Callback = callback;
}

void Logger::Log(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (level < s_Level) return;

    std::lock_guard<std::mutex> lock(s_LogMutex);

    // 格式化消息
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    time_t now = time(nullptr);
    struct tm* t = localtime(&now);

    SetConsoleColor(level);
    printf("[%02d:%02d:%02d] [%s] %s\n",
        t->tm_hour, t->tm_min, t->tm_sec,
        LevelToString(level), buffer);
    ResetConsoleColor();

    // 转发到回调 (Console)
    if (s_Callback) {
        s_Callback(level, buffer);
    }

    if (level >= LogLevel::Warn) {
        fflush(stdout);
    }
}

} // namespace Engine
