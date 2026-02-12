#pragma once

#include "engine/core/log.h"

// ── 断言宏 ──────────────────────────────────────────────────

#ifdef ENGINE_DEBUG
    #define ENGINE_ASSERT(condition, ...)                               \
        do {                                                           \
            if (!(condition)) {                                        \
                LOG_FATAL("断言失败: %s | 文件: %s | 行: %d",           \
                    #condition, __FILE__, __LINE__);                   \
                __builtin_trap();                                      \
            }                                                          \
        } while(0)
#else
    #define ENGINE_ASSERT(condition, ...) ((void)0)
#endif
