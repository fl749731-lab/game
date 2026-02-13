"""
生成器 — 周期性创建实体
挂到一个实体上，它会在自身位置附近定时生成新实体

用途: 敌人生成点、粒子发射器、道具刷新点
"""
import engine_api as api
import json
import math
import random


def on_create(entity_id, ctx_json):
    """初始化生成器参数"""
    api.log_info("[Spawner] 生成器已创建")
    api.set_var(entity_id, "timer", 0)
    api.set_var(entity_id, "interval", 5.0)     # 生成间隔 (秒)
    api.set_var(entity_id, "max_count", 10)     # 最大同时存在数
    api.set_var(entity_id, "spawned", 0)        # 已生成数
    api.set_var(entity_id, "radius", 5.0)       # 生成半径
    api.set_var(entity_id, "active", 1.0)       # 是否激活
    api.set_string_var(entity_id, "spawn_name", "SpawnedEntity")
    api.set_string_var(entity_id, "spawn_script", "")  # 生成物的脚本


def on_update(entity_id, dt, ctx_json):
    """每帧检查是否需要生成"""
    ctx = json.loads(ctx_json)
    dt = float(dt)

    active = api.get_var(entity_id, "active", 1.0)
    if active < 0.5:
        return

    timer = api.get_var(entity_id, "timer", 0) + dt
    interval = api.get_var(entity_id, "interval", 5.0)

    if timer >= interval:
        timer = 0
        spawned = int(api.get_var(entity_id, "spawned", 0))
        max_count = int(api.get_var(entity_id, "max_count", 10))

        if spawned < max_count:
            pos = ctx.get("pos", [0, 0, 0])
            radius = api.get_var(entity_id, "radius", 5.0)
            name = api.get_string_var(entity_id, "spawn_name", "SpawnedEntity")
            script = api.get_string_var(entity_id, "spawn_script", "")

            # 随机偏移位置
            angle = random.uniform(0, 2 * math.pi)
            ox = math.cos(angle) * random.uniform(0, radius)
            oz = math.sin(angle) * random.uniform(0, radius)

            new_id = api.spawn_entity(name, pos[0] + ox, pos[1], pos[2] + oz, script)
            if new_id > 0:
                api.set_var(entity_id, "spawned", spawned + 1)
                api.log_info(f"[Spawner] 生成 {name} (ID={new_id}) 在 ({pos[0]+ox:.1f}, {pos[1]:.1f}, {pos[2]+oz:.1f})")

    api.set_var(entity_id, "timer", timer)


def on_event(entity_id, event_json):
    """响应事件"""
    event = json.loads(event_json)
    evt_type = event.get("type", "")

    if evt_type == "wave_start":
        # 波次触发，重置计时器并立即生成
        data = event.get("data", {})
        count = data.get("count", 3)
        api.set_var(entity_id, "timer", 999)  # 强制下帧触发
        api.set_var(entity_id, "max_count", count)
        api.set_var(entity_id, "spawned", 0)

    elif evt_type == "deactivate":
        api.set_var(entity_id, "active", 0)


def on_destroy(entity_id):
    api.log_info("[Spawner] 生成器已销毁")


def get_script_info():
    return {
        "name": "生成器",
        "type": "logic",
        "description": "周期性在自身位置附近创建新实体"
    }
