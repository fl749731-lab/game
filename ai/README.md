# 脚本逻辑层

## 概述

引擎的 Python 脚本系统分为两层：

- **ScriptComponent + ScriptSystem** — 通用脚本逻辑层，任何实体可挂载 Python 脚本
- **AIComponent + AIManager** — AI 行为专用层（基于 ScriptComponent 的高级封装）
- **层级指挥链** — 指挥官 → 小队长 → 士兵 三层决策，命令逐层下发

```text
C++ Engine
  ├── ScriptSystem ──→ ScriptComponent (通用逻辑)
  │     ↕ PythonEngine
  │   Python 脚本 (engine_api.py ↔ C++ 引擎功能)
  │
  └── AIManager (三阶段更新)
        │
        ├─ Phase 1: Commander AI    ─→ 全局战术 + 玩家意图识别
        │    ↓ 命令下发
        ├─ Phase 2: Squad Leader AI ─→ 小队战术 + 阵型管理
        │    ↓ 子命令下发
        └─ Phase 3: Soldier AI      ─→ 个体执行 + 本地自主
```

## 脚本生命周期

| 回调 | 时机 | 签名 |
|------|------|------|
| `on_create` | 首次执行 | `on_create(entity_id, ctx_json)` |
| `on_update` | 每帧 | `on_update(entity_id, dt, ctx_json)` |
| `on_event` | 事件触发 | `on_event(entity_id, event_json)` |
| `on_destroy` | 销毁前 | `on_destroy(entity_id)` |

## Engine API

`engine_api.py` 提供 30+ 个 Python 函数调用 C++ 引擎功能：

| 类别 | 函数示例 |
|------|---------|
| Transform | `get_position` / `set_position` / `get_rotation` / `set_scale` |
| Physics | `add_force` / `add_impulse` / `get_velocity` |
| Entity | `spawn_entity` / `destroy_entity` / `find_by_tag` / `find_nearby` |
| Component | `get_health` / `set_health` / `get_var` / `set_var` |
| Event | `send_event` |
| Audio | `play_sound` / `stop_sound` |
| Debug | `log_info` / `log_warn` / `log_error` |

## 现有脚本

### 逻辑脚本

| 脚本 | 文件 | 用途 |
|------|------|------|
| 游戏管理器 | `game_manager.py` | 游戏流程/波次/分数管理 |
| 生成器 | `spawner.py` | 定时在周围生成新实体 |
| 可拾取物品 | `collectible.py` | 浮动动画 + 自动拾取 + 效果触发 |
| 触发区域 | `trigger_zone.py` | 进入区域发送自定义事件 |
| 投射物 | `projectile.py` | 沿方向飞行 + 碰撞伤害 |

### AI 脚本 — 基础

| 脚本 | 文件 | 行为 |
|------|------|------|
| 默认 AI | `default_ai.py` | 基础巡逻 + 追击逻辑 |
| 攻击型 | `aggressive_ai.py` | 主动寻找并追击敌人 |
| 防御型 | `defensive_ai.py` | 保持据点，受攻击才反击 |

### AI 脚本 — 层级指挥链

| 脚本 | 文件 | 角色 | 能力 |
|------|------|------|------|
| 指挥官 AI | `commander_ai.py` | Commander | 玩家意图识别 (6种模式) + 长期记忆 + 战术矩阵 |
| 小队长 AI | `squad_leader_ai.py` | Leader | 接收命令→分解子命令 + 阵型管理 (4种阵型) |
| 智能士兵 AI | `smart_soldier_ai.py` | Soldier | 执行命令 + 阵型保持 + 集火协作 + 自我保护 |

**使用方式**: 给实体添加 `SquadComponent` + `AIComponent`，设置 `Role`/`SquadID`/`ScriptModule`。

### 工具库

- `ai_utils.py` — 距离计算、方向向量、阵型位置计算 (4种)、命令构建/解析、优先级目标选择
- `engine_api.py` — 引擎功能 API 封装

## 开发新脚本

在 `ai/scripts/` 下创建 `.py` 文件，实现生命周期方法：

```python
import engine_api as api

def on_create(entity_id, ctx_json):
    api.log_info("脚本已创建")
    api.set_var(entity_id, "timer", 0)

def on_update(entity_id, dt, ctx_json):
    timer = api.get_var(entity_id, "timer", 0) + float(dt)
    api.set_var(entity_id, "timer", timer)
    # 你的逻辑...

def on_event(entity_id, event_json):
    pass

def on_destroy(entity_id):
    pass
```

在 C++ 中挂载到实体：

```cpp
auto e = scene.CreateEntity("MyEntity");
auto& sc = world.AddComponent<Engine::ScriptComponent>(e);
sc.ScriptModule = "my_script";  // 对应 ai/scripts/my_script.py
```

## 构建启用

```bash
cmake -B build -G "MinGW Makefiles" -DENGINE_ENABLE_PYTHON=ON
```

需要安装 Python 3 开发包。无 Python 时脚本系统有完整 stub 实现，引擎功能不受影响。
