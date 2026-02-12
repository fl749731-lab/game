"""
默认 AI — 巡逻型
- Idle → Patrol：开始巡逻
- Patrol：沿路径点移动，检测敌人
- Chase：朝最近敌人移动
- Attack：在攻击范围内攻击
- Flee：血量低时逃跑
- Dead：死亡
"""
from ai_utils import *


def update_ai(ctx_json):
    ctx = parse_context(ctx_json)

    state = ctx['state']
    pos = ctx['pos']
    health = ctx['health']
    max_health = ctx['max_health']
    dt = ctx['dt']
    enemies = ctx['enemies']
    patrol_points = ctx.get('patrol_points', [])
    patrol_idx = ctx.get('patrol_index', 0)
    attack_range = ctx['attack_range']
    move_speed = ctx['move_speed']

    # 死亡
    if health <= 0:
        return make_action("Dead")

    # 低血量逃跑 (< 20%)
    if health < max_health * 0.2:
        if enemies:
            # 逃离最近敌人
            enemy = enemies[0]
            away = direction_to(enemy['pos'], pos)
            return make_action("Flee", away[0], away[1], away[2], move_speed * 1.3)
        return make_action("Flee", 0, 0, 1, move_speed * 1.3)

    # ── 状态逻辑 ────────────────────────────

    if state == "Idle":
        # 发现敌人 → 追击
        if enemies:
            return make_action("Chase")
        # 有巡逻点 → 巡逻
        if patrol_points:
            return make_action("Patrol")
        return make_action("Idle")

    elif state == "Patrol":
        # 发现敌人 → 追击
        if enemies:
            return make_action("Chase")

        # 沿路径点移动
        if patrol_points:
            target = patrol_points[patrol_idx % len(patrol_points)]
            dist = distance_xz(pos, target)

            if dist < 1.0:
                # 到达路径点 → 下一个
                return make_action("Patrol", custom=f"next_patrol")
            else:
                d = direction_to(pos, target)
                return make_action("Patrol", d[0], d[1], d[2], move_speed * 0.6)

        return make_action("Idle")

    elif state == "Chase":
        if not enemies:
            return make_action("Patrol")

        enemy = choose_closest_enemy(ctx)
        dist = enemy['dist']

        if dist <= attack_range:
            return make_action("Attack", target=enemy['id'])

        # 追击
        d = direction_to(pos, enemy['pos'])
        return make_action("Chase", d[0], d[1], d[2], move_speed)

    elif state == "Attack":
        if not enemies:
            return make_action("Patrol")

        enemy = choose_closest_enemy(ctx)
        if enemy['dist'] > attack_range * 1.5:
            return make_action("Chase")

        # 持续攻击（面向敌人但不移动）
        d = direction_to(pos, enemy['pos'])
        return make_action("Attack", d[0], d[1], d[2], 0, target=enemy['id'])

    elif state == "Flee":
        if health > max_health * 0.4:
            return make_action("Patrol")
        if enemies:
            enemy = enemies[0]
            away = direction_to(enemy['pos'], pos)
            return make_action("Flee", away[0], away[1], away[2], move_speed * 1.3)
        return make_action("Idle")

    return make_action(state)


def get_ai_info():
    return {
        "name": "默认巡逻 AI",
        "version": "2.0",
        "states": ["Idle", "Patrol", "Chase", "Attack", "Flee", "Dead"],
        "description": "沿路径点巡逻，发现敌人追击，近距离攻击，低血量逃跑"
    }
