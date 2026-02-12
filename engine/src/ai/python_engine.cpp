#ifdef ENGINE_HAS_PYTHON

#include "engine/ai/python_engine.h"

// Python C API
#include <Python.h>

#include <sstream>

namespace Engine {
namespace AI {

bool PythonEngine::s_Initialized = false;
std::string PythonEngine::s_LastError;

bool PythonEngine::Init(const std::string& scriptsPath) {
    if (s_Initialized) {
        LOG_WARN("[AI] Python 引擎已经初始化");
        return true;
    }

    LOG_INFO("[AI] 正在初始化 Python 解释器...");

    // 初始化 Python
    Py_Initialize();

    if (!Py_IsInitialized()) {
        s_LastError = "Python 解释器初始化失败";
        LOG_ERROR("[AI] %s", s_LastError.c_str());
        return false;
    }

    // 将脚本路径添加到 sys.path
    std::string pathCmd = "import sys; sys.path.insert(0, '" + scriptsPath + "')";
    PyRun_SimpleString(pathCmd.c_str());

    s_Initialized = true;
    
    // 获取 Python 版本
    const char* version = Py_GetVersion();
    LOG_INFO("[AI] Python %s 已就绪", version);
    LOG_INFO("[AI] 脚本目录: %s", scriptsPath.c_str());

    return true;
}

void PythonEngine::Shutdown() {
    if (!s_Initialized) return;

    LOG_INFO("[AI] 关闭 Python 解释器...");
    Py_Finalize();
    s_Initialized = false;
}

bool PythonEngine::IsInitialized() {
    return s_Initialized;
}

bool PythonEngine::Execute(const std::string& code) {
    if (!s_Initialized) {
        s_LastError = "Python 未初始化";
        return false;
    }

    int result = PyRun_SimpleString(code.c_str());
    if (result != 0) {
        s_LastError = "Python 执行失败";
        LOG_ERROR("[AI] %s: %s", s_LastError.c_str(), code.c_str());
        return false;
    }
    return true;
}

bool PythonEngine::ExecuteFile(const std::string& filepath) {
    if (!s_Initialized) {
        s_LastError = "Python 未初始化";
        return false;
    }

    FILE* fp = fopen(filepath.c_str(), "r");
    if (!fp) {
        s_LastError = "无法打开脚本文件: " + filepath;
        LOG_ERROR("[AI] %s", s_LastError.c_str());
        return false;
    }

    int result = PyRun_SimpleFile(fp, filepath.c_str());
    fclose(fp);

    if (result != 0) {
        s_LastError = "脚本执行失败: " + filepath;
        LOG_ERROR("[AI] %s", s_LastError.c_str());
        return false;
    }
    return true;
}

std::string PythonEngine::CallFunction(const std::string& module,
                                        const std::string& func,
                                        const std::vector<std::string>& args) {
    if (!s_Initialized) {
        s_LastError = "Python 未初始化";
        return "";
    }

    // 导入模块
    PyObject* pModuleName = PyUnicode_FromString(module.c_str());
    PyObject* pModule = PyImport_Import(pModuleName);
    Py_DECREF(pModuleName);

    if (!pModule) {
        PyErr_Print();
        s_LastError = "无法导入模块: " + module;
        LOG_ERROR("[AI] %s", s_LastError.c_str());
        return "";
    }

    // 获取函数
    PyObject* pFunc = PyObject_GetAttrString(pModule, func.c_str());
    if (!pFunc || !PyCallable_Check(pFunc)) {
        PyErr_Print();
        Py_DECREF(pModule);
        s_LastError = "找不到函数: " + module + "." + func;
        LOG_ERROR("[AI] %s", s_LastError.c_str());
        return "";
    }

    // 构建参数元组
    PyObject* pArgs = PyTuple_New((Py_ssize_t)args.size());
    for (size_t i = 0; i < args.size(); i++) {
        PyTuple_SetItem(pArgs, (Py_ssize_t)i, PyUnicode_FromString(args[i].c_str()));
    }

    // 调用
    PyObject* pResult = PyObject_CallObject(pFunc, pArgs);

    Py_DECREF(pArgs);
    Py_DECREF(pFunc);
    Py_DECREF(pModule);

    if (!pResult) {
        PyErr_Print();
        s_LastError = "函数调用失败: " + module + "." + func;
        LOG_ERROR("[AI] %s", s_LastError.c_str());
        return "";
    }

    // 将结果转为字符串
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

    if (!pModule) {
        PyErr_Print();
        return "";
    }

    PyObject* pVar = PyObject_GetAttrString(pModule, varName.c_str());
    Py_DECREF(pModule);

    if (!pVar) {
        PyErr_Print();
        return "";
    }

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

std::string PythonEngine::GetLastError() {
    return s_LastError;
}

// ── AI State ────────────────────────────────────────────────

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

// ── AI Agent ────────────────────────────────────────────────

void AIAgent::UpdateAI(f32 dt) {
    if (!PythonEngine::IsInitialized()) return;

    // 构建参数：entity_id, state, health, dt
    std::vector<std::string> args = {
        std::to_string(EntityID),
        AIStateToString(State),
        std::to_string(Health),
        std::to_string(dt)
    };

    std::string result = PythonEngine::CallFunction(
        ScriptModule, "update_ai", args
    );

    // 解析返回的新状态
    if (result == "Idle")    State = AIState::Idle;
    else if (result == "Patrol") State = AIState::Patrol;
    else if (result == "Chase")  State = AIState::Chase;
    else if (result == "Attack") State = AIState::Attack;
    else if (result == "Flee")   State = AIState::Flee;
    else if (result == "Dead")   State = AIState::Dead;
}

} // namespace AI
} // namespace Engine

#endif // ENGINE_HAS_PYTHON
