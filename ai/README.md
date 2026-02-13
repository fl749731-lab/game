# Python AI 模块

## 概述

引擎通过 [pybind11](https://github.com/pybind/pybind11) 将 C++ AI 上下文暴露给 Python，实现灵活的 AI 行为脚本。

## 架构

```text
C++ Engine (AIManager)
    ↕  pybind11
Python AI Scripts (ai/scripts/)
```

### 数据流

1. C++ `AIManager` 收集实体的 AI 上下文（位置、血量、附近敌人等）
2. 通过 pybind11 调用对应的 Python AI 脚本
3. Python 脚本根据上下文返回决策（移动/攻击/防御等）
4. C++ 引擎执行决策

## 现有脚本

| 脚本 | 文件 | 行为 |
| ------ | ------ | ------ |
| 默认 AI | `default_ai.py` | 基础巡逻 + 追击逻辑 |
| 攻击型 | `aggressive_ai.py` | 主动寻找并追击敌人 |
| 防御型 | `defensive_ai.py` | 保持据点，受攻击才反击 |

## 工具函数

`ai_utils.py` 提供常用工具：

- 距离计算
- 方向向量
- 范围检测
- 随机巡逻点生成

## 开发新 AI 脚本

创建一个新的 `.py` 文件在 `ai/scripts/` 目录下：

```python
def decide(context):
    """
    AI 决策入口

    Args:
        context: 包含以下字段的对象
            - position: (x, y, z) 当前位置
            - health: 当前血量
            - max_health: 最大血量
            - enemies: 附近敌人列表
            - state: 当前状态字符串

    Returns:
        dict: 包含 action 和相关参数
            - action: "move" / "attack" / "idle" / "flee"
            - target: 目标位置或实体
    """
    # 你的 AI 逻辑
    return {"action": "idle"}
```

然后在实体的 `AIComponent` 中设置 `ScriptModule` 为你的脚本名（不含 `.py`）。

## 构建启用

```bash
cmake -B build -G "MinGW Makefiles" -DENGINE_ENABLE_PYTHON=ON
```

需要安装 Python 3 开发包。
