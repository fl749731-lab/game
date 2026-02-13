"""
游戏管理器 — 控制游戏流程和全局状态
挂到一个空实体上作为游戏控制器

功能:
- 管理游戏状态 (菜单/游戏中/暂停/结束)
- 跟踪分数、时间、波次
- 控制敌人生成节奏
"""
import engine_api as api
import json

# 游戏状态
STATE_MENU = "menu"
STATE_PLAYING = "playing"
STATE_PAUSED = "paused"
STATE_GAMEOVER = "gameover"


def on_create(entity_id, ctx_json):
    """游戏初始化"""
    api.log_info("[GameManager] 游戏管理器已创建")
    api.set_var(entity_id, "score", 0)
    api.set_var(entity_id, "wave", 1)
    api.set_var(entity_id, "wave_timer", 0)
    api.set_var(entity_id, "wave_interval", 10.0)  # 每 10 秒新波次
    api.set_var(entity_id, "enemies_per_wave", 3)
    api.set_string_var(entity_id, "state", STATE_PLAYING)


def on_update(entity_id, dt, ctx_json):
    """每帧更新"""
    ctx = json.loads(ctx_json)
    dt = float(dt)

    state = api.get_string_var(entity_id, "state", STATE_PLAYING)
    if state != STATE_PLAYING:
        return

    # 更新波次计时器
    timer = api.get_var(entity_id, "wave_timer", 0)
    timer += dt
    interval = api.get_var(entity_id, "wave_interval", 10.0)

    if timer >= interval:
        # 新波次
        wave = int(api.get_var(entity_id, "wave", 1))
        wave += 1
        api.set_var(entity_id, "wave", wave)
        api.set_var(entity_id, "wave_timer", 0)

        enemies = int(api.get_var(entity_id, "enemies_per_wave", 3))
        api.log_info(f"[GameManager] 波次 {wave} 开始! 生成 {enemies} 个敌人")

        # 通知生成器
        api.send_event("wave_start", 0, json.dumps({
            "wave": wave,
            "count": enemies
        }))
    else:
        api.set_var(entity_id, "wave_timer", timer)


def on_event(entity_id, event_json):
    """处理事件"""
    event = json.loads(event_json)
    evt_type = event.get("type", "")

    if evt_type == "enemy_killed":
        score = api.get_var(entity_id, "score", 0) + 10
        api.set_var(entity_id, "score", score)
        api.log_info(f"[GameManager] 得分: {score}")

    elif evt_type == "player_died":
        api.set_string_var(entity_id, "state", STATE_GAMEOVER)
        api.log_info("[GameManager] 游戏结束!")


def on_destroy(entity_id):
    """关闭"""
    api.log_info("[GameManager] 游戏管理器已销毁")


def get_script_info():
    return {
        "name": "游戏管理器",
        "type": "logic",
        "description": "控制游戏流程、波次、分数"
    }
