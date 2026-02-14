#pragma once

#include "engine/core/types.h"

// pybind11 桥接: 向 Python 暴露引擎 AI / ECS / 物理 API
// 需要 ENGINE_ENABLE_PYTHON=ON 编译

#ifdef ENGINE_ENABLE_PYTHON

namespace Engine {

class PythonBridge {
public:
    /// 初始化 Python 解释器并注册引擎模块
    static bool Init();

    /// 关闭 Python 解释器
    static void Shutdown();

    /// 执行 Python 脚本文件
    static bool ExecuteFile(const char* filepath);

    /// 执行 Python 代码字符串
    static bool ExecuteString(const char* code);

    /// 调用 Python 函数 (模块名.函数名)
    static bool CallFunction(const char* moduleName, const char* funcName);

    /// 是否已初始化
    static bool IsInitialized();

    /// 每帧 Tick (调用 Python 端注册的 update 回调)
    static void Tick(f32 dt);

private:
    static bool s_Initialized;
};

} // namespace Engine

#endif // ENGINE_ENABLE_PYTHON
