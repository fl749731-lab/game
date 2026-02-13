"""
触发区域 — 实体进入/离开区域时触发事件
挂到一个带碰撞体的实体上

用途: 关卡触发器、传送门、危险区域、剧情触发
"""
import engine_api as api
import json


def on_create(entity_id, ctx_json):
    """初始化触发区域"""
    api.set_var(entity_id, "trigger_radius", 3.0)  # 触发半径
    api.set_var(entity_id, "cooldown", 2.0)         # 冷却时间
    api.set_var(entity_id, "timer", 0)
    api.set_var(entity_id, "one_shot", 0)            # 1=只触发一次
    api.set_var(entity_id, "triggered", 0)
    api.set_var(entity_id, "active", 1.0)
    api.set_string_var(entity_id, "event_type", "zone_enter")
    api.set_string_var(entity_id, "filter_tag", "")  # 空=任意实体


def on_update(entity_id, dt, ctx_json):
    """每帧检测进入区域的实体"""
    dt = float(dt)
    active = api.get_var(entity_id, "active", 1.0)
    if active < 0.5:
        return

    one_shot = api.get_var(entity_id, "one_shot", 0) > 0.5
    triggered = api.get_var(entity_id, "triggered", 0) > 0.5
    if one_shot and triggered:
        return

    # 冷却
    timer = api.get_var(entity_id, "timer", 0)
    if timer > 0:
        api.set_var(entity_id, "timer", timer - dt)
        return

    radius = api.get_var(entity_id, "trigger_radius", 3.0)
    filter_tag = api.get_string_var(entity_id, "filter_tag", "")
    event_type = api.get_string_var(entity_id, "event_type", "zone_enter")

    nearby = api.find_nearby(entity_id, radius)
    for entity in nearby:
        name = entity.get("name", "")
        if filter_tag and name != filter_tag:
            continue

        # 触发!
        api.send_event(event_type, entity["id"], json.dumps({
            "zone_id": entity_id,
            "entity_id": entity["id"],
            "entity_name": name
        }))
        api.log_info(f"[TriggerZone] {name} (ID={entity['id']}) 进入触发区域")

        cooldown = api.get_var(entity_id, "cooldown", 2.0)
        api.set_var(entity_id, "timer", cooldown)

        if one_shot:
            api.set_var(entity_id, "triggered", 1)
        break


def on_event(entity_id, event_json):
    """处理事件 (如重置触发器)"""
    event = json.loads(event_json)
    if event.get("type") == "reset":
        api.set_var(entity_id, "triggered", 0)
        api.set_var(entity_id, "timer", 0)


def on_destroy(entity_id):
    pass


def get_script_info():
    return {
        "name": "触发区域",
        "type": "logic",
        "description": "实体进入区域时触发自定义事件"
    }
