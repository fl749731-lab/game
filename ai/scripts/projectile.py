"""
投射物 — 沿方向飞行，碰撞时造成伤害并销毁
由其他脚本通过 spawn_entity 生成

用途: 子弹、箭矢、魔法弹
"""
import engine_api as api
import json
import math


def on_create(entity_id, ctx_json):
    """初始化投射物"""
    api.set_var(entity_id, "speed", 15.0)        # 飞行速度
    api.set_var(entity_id, "damage", 10.0)       # 伤害
    api.set_var(entity_id, "lifetime", 5.0)      # 最大存活时间
    api.set_var(entity_id, "timer", 0)
    api.set_var(entity_id, "dir_x", 0)           # 方向 (由生成者设置)
    api.set_var(entity_id, "dir_y", 0)
    api.set_var(entity_id, "dir_z", 1)
    api.set_var(entity_id, "owner_id", 0)        # 发射者 ID
    api.set_var(entity_id, "hit_radius", 0.5)    # 命中判定半径


def on_update(entity_id, dt, ctx_json):
    """每帧: 移动 + 碰撞检测"""
    ctx = json.loads(ctx_json)
    dt = float(dt)
    pos = ctx.get("pos", [0, 0, 0])

    # 生命周期
    timer = api.get_var(entity_id, "timer", 0) + dt
    lifetime = api.get_var(entity_id, "lifetime", 5.0)
    if timer > lifetime:
        api.destroy_entity(entity_id)
        return
    api.set_var(entity_id, "timer", timer)

    # 移动
    speed = api.get_var(entity_id, "speed", 15.0)
    dx = api.get_var(entity_id, "dir_x", 0)
    dy = api.get_var(entity_id, "dir_y", 0)
    dz = api.get_var(entity_id, "dir_z", 1)

    new_x = pos[0] + dx * speed * dt
    new_y = pos[1] + dy * speed * dt
    new_z = pos[2] + dz * speed * dt
    api.set_position(entity_id, new_x, new_y, new_z)

    # 碰撞检测
    hit_radius = api.get_var(entity_id, "hit_radius", 0.5)
    owner_id = int(api.get_var(entity_id, "owner_id", 0))
    nearby = api.find_nearby(entity_id, hit_radius)

    for entity in nearby:
        eid = entity.get("id", 0)
        if eid == owner_id:
            continue  # 不打自己

        # 命中!
        damage = api.get_var(entity_id, "damage", 10.0)
        hp, max_hp = api.get_health(eid)
        if max_hp > 0:
            new_hp = max(0, hp - damage)
            api.set_health(eid, new_hp, max_hp)
            api.log_info(f"[Projectile] 命中 {entity.get('name','?')} 造成 {damage} 伤害")

            if new_hp <= 0:
                api.send_event("enemy_killed", 0, json.dumps({"entity_id": eid}))

        api.destroy_entity(entity_id)
        return


def on_event(entity_id, event_json):
    """碰撞事件"""
    event = json.loads(event_json)
    if event.get("type") == "collision":
        api.destroy_entity(entity_id)


def on_destroy(entity_id):
    pass


def get_script_info():
    return {
        "name": "投射物",
        "type": "logic",
        "description": "沿方向飞行，碰撞造成伤害并自毁"
    }
