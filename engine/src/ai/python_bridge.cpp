#ifdef ENGINE_ENABLE_PYTHON

#include "engine/ai/python_bridge.h"
#include "engine/ai/behavior_tree.h"
#include "engine/core/log.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/embed.h>

namespace py = pybind11;

namespace Engine {

bool PythonBridge::s_Initialized = false;

// ── 引擎模块定义 ────────────────────────────────────────────

PYBIND11_EMBEDDED_MODULE(engine, m) {
    m.doc() = "Engine Python Bridge";

    // 暴露 BTStatus
    py::enum_<BTStatus>(m, "BTStatus")
        .value("Success", BTStatus::Success)
        .value("Failure", BTStatus::Failure)
        .value("Running", BTStatus::Running);

    // 暴露 BTAction
    py::class_<BTAction, BTNode, std::shared_ptr<BTAction>>(m, "Action")
        .def(py::init<const std::string&, BTAction::ActionFunc>())
        .def("tick", &BTAction::Tick);

    // 暴露 BTCondition
    py::class_<BTCondition, BTNode, std::shared_ptr<BTCondition>>(m, "Condition")
        .def(py::init<const std::string&, BTCondition::CondFunc>())
        .def("tick", &BTCondition::Tick);

    // 暴露 BTSequence
    py::class_<BTSequence, BTNode, std::shared_ptr<BTSequence>>(m, "Sequence")
        .def(py::init<>())
        .def("add_child", &BTSequence::AddChild)
        .def("tick", &BTSequence::Tick);

    // 暴露 BTSelector
    py::class_<BTSelector, BTNode, std::shared_ptr<BTSelector>>(m, "Selector")
        .def(py::init<>())
        .def("add_child", &BTSelector::AddChild)
        .def("tick", &BTSelector::Tick);

    // 暴露 BehaviorTree
    py::class_<BehaviorTree>(m, "BehaviorTree")
        .def(py::init<>())
        .def("set_root", &BehaviorTree::SetRoot)
        .def("tick", &BehaviorTree::Tick)
        .def("reset", &BehaviorTree::Reset);

    // 暴露 NavGrid
    py::class_<NavGrid>(m, "NavGrid")
        .def(py::init<u32, u32, f32>(),
             py::arg("width"), py::arg("height"), py::arg("cell_size") = 1.0f)
        .def("set_walkable", &NavGrid::SetWalkable)
        .def("is_walkable", &NavGrid::IsWalkable)
        .def("find_path", &NavGrid::FindPath);
}

// ── Python 桥接实现 ─────────────────────────────────────────

bool PythonBridge::Init() {
    if (s_Initialized) return true;

    try {
        py::initialize_interpreter();
        s_Initialized = true;
        LOG_INFO("[PythonBridge] Python 解释器已初始化");

        // 将 AI 脚本目录添加到 Python 路径
        py::module_::import("sys").attr("path").cast<py::list>().append("ai/scripts");
        return true;
    } catch (const py::error_already_set& e) {
        LOG_ERROR("[PythonBridge] Python 初始化失败: %s", e.what());
        return false;
    }
}

void PythonBridge::Shutdown() {
    if (!s_Initialized) return;
    py::finalize_interpreter();
    s_Initialized = false;
    LOG_DEBUG("[PythonBridge] Python 解释器已关闭");
}

bool PythonBridge::ExecuteFile(const char* filepath) {
    if (!s_Initialized) return false;
    try {
        py::eval_file(filepath);
        return true;
    } catch (const py::error_already_set& e) {
        LOG_ERROR("[PythonBridge] 执行文件 '%s' 失败: %s", filepath, e.what());
        return false;
    }
}

bool PythonBridge::ExecuteString(const char* code) {
    if (!s_Initialized) return false;
    try {
        py::exec(code);
        return true;
    } catch (const py::error_already_set& e) {
        LOG_ERROR("[PythonBridge] 执行代码失败: %s", e.what());
        return false;
    }
}

bool PythonBridge::CallFunction(const char* moduleName, const char* funcName) {
    if (!s_Initialized) return false;
    try {
        auto mod = py::module_::import(moduleName);
        mod.attr(funcName)();
        return true;
    } catch (const py::error_already_set& e) {
        LOG_ERROR("[PythonBridge] 调用 %s.%s() 失败: %s", moduleName, funcName, e.what());
        return false;
    }
}

bool PythonBridge::IsInitialized() { return s_Initialized; }

void PythonBridge::Tick(f32 dt) {
    if (!s_Initialized) return;
    try {
        auto mod = py::module_::import("ai_main");
        if (py::hasattr(mod, "update")) {
            mod.attr("update")(dt);
        }
    } catch (const py::error_already_set&) {
        // ai_main 模块不存在时静默忽略
    }
}

} // namespace Engine

#endif // ENGINE_ENABLE_PYTHON
