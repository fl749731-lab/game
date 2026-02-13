"""
可拾取物品 — 碰到即消失并触发效果
挂到物品实体上，当玩家接近时自动拾取

用途: 金币、血瓶、弹药、经验球
"""
import engine_api as api
import json
import math


def on_create(entity_id, ctx_json):
    """初始化物品参数"""
    api.set_var(entity_id, "pickup_radius", 1.5)   # 拾取半径
    api.set_var(entity_id, "value", 1.0)            # 效果值
    api.set_var(entity_id, "bob_speed", 2.0)        # 上下浮动速度
    api.set_var(entity_id, "bob_height", 0.3)       # 浮动幅度
    api.set_var(entity_id, "spin_speed", 90.0)      # 旋转速度 (度/秒)
    api.set_var(entity_id, "lifetime", 30.0)        # 存在时间 (秒, 0=永久)
    api.set_var(entity_id, "timer", 0)
    api.set_string_var(entity_id, "effect", "score")  # score/health/ammo
    api.set_string_var(entity_id, "pickup_tag", "Player")  # 谁能拾取


def on_update(entity_id, dt, ctx_json):
    """每帧: 浮动动画 + 旋转 + 检测拾取"""
    ctx = json.loads(ctx_json)
    dt = float(dt)
    pos = ctx.get("pos", [0, 0, 0])

    # 时间
    timer = api.get_var(entity_id, "timer", 0) + dt
    api.set_var(entity_id, "timer", timer)

    # 生命周期
    lifetime = api.get_var(entity_id, "lifetime", 30.0)
    if lifetime > 0 and timer > lifetime:
        api.destroy_entity(entity_id)
        return

    # 浮动动画
    bob_speed = api.get_var(entity_id, "bob_speed", 2.0)
    bob_height = api.get_var(entity_id, "bob_height", 0.3)
    base_y = api.get_var(entity_id, "base_y", pos[1])
    if timer < dt * 2:
        api.set_var(entity_id, "base_y", pos[1])
        base_y = pos[1]
    new_y = base_y + math.sin(timer * bob_speed) * bob_height
    api.set_position(entity_id, pos[0], new_y, pos[2])

    # 旋转
    spin = api.get_var(entity_id, "spin_speed", 90.0)
    rot = ctx.get("rot", [0, 0, 0])
    api.set_rotation(entity_id, rot[0], rot[1] + spin * dt, rot[2])

    # 检测附近实体
    pickup_radius = api.get_var(entity_id, "pickup_radius", 1.5)
    pickup_tag = api.get_string_var(entity_id, "pickup_tag", "Player")
    nearby = api.find_nearby(entity_id, pickup_radius)

    for entity in nearby:
        if entity.get("name", "") == pickup_tag:
            _pickup(entity_id, entity["id"])
            return


def _pickup(item_id, picker_id):
    """执行拾取"""
    effect = api.get_string_var(item_id, "effect", "score")
    value = api.get_var(item_id, "value", 1.0)

    if effect == "health":
        hp, max_hp = api.get_health(picker_id)
        new_hp = min(hp + value, max_hp)
        api.set_health(picker_id, new_hp)
        api.log_info(f"[Collectible] {picker_id} 回复 {value} 生命")
    elif effect == "score":
        api.send_event("score_gained", 0, json.dumps({"value": value}))
        api.log_info(f"[Collectible] 得分 +{value}")

    api.play_sound("pickup")
    api.destroy_entity(item_id)


def on_destroy(entity_id):
    pass


def get_script_info():
    return {
        "name": "可拾取物品",
        "type": "logic",
        "description": "接近自动拾取，触发回血/加分等效果"
    }
