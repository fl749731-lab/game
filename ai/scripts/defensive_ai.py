"""
防御型 AI — 守卫固定位置
- 守在出生点附近
- 仅当敌人进入检测范围时反应
- 追击距离有限，超出返回守卫点
- 血量低时提前撤退到守卫点
"""
from ai_utils import *

# 每个实体的守卫点缓存
_guard_positions = {}


def update_ai(ctx_json):
    ctx = parse_context(ctx_json)

    entity_id = ctx['entity_id']
    pos = ctx['pos']
    health = ctx['health']
    max_health = ctx['max_health']
    enemies = ctx['enemies']
    attack_range = ctx['attack_range']
    detect_range = ctx['detect_range']
    move_speed = ctx['move_speed']
    state = ctx['state']

    if health <= 0:
        return make_action("Dead")

    # 记录初始位置（守卫点）
    if entity_id not in _guard_positions:
        _guard_positions[entity_id] = list(pos)

    guard_pos = _guard_positions[entity_id]
    dist_from_guard = distance_xz(pos, guard_pos)

    # 最大追击距离 = 检测范围 * 1.5
    max_chase_dist = detect_range * 1.5

    # 低血量 → 撤退到守卫点
    if health < max_health * 0.3:
        if dist_from_guard > 2.0:
            d = direction_to(pos, guard_pos)
            return make_action("Flee", d[0], d[1], d[2], move_speed)
        return make_action("Idle")

    # 超出追击范围 → 返回守卫点
    if dist_from_guard > max_chase_dist:
        d = direction_to(pos, guard_pos)
        return make_action("Patrol", d[0], d[1], d[2], move_speed * 0.8, custom="returning")

    # 没有敌人 → 返回守卫点或待命
    if not enemies:
        if dist_from_guard > 2.0:
            d = direction_to(pos, guard_pos)
            return make_action("Patrol", d[0], d[1], d[2], move_speed * 0.6, custom="returning")
        return make_action("Idle")

    # 有敌人
    enemy = choose_closest_enemy(ctx)
    dist = enemy['dist']
    d = direction_to(pos, enemy['pos'])

    # 攻击范围内 → 攻击
    if dist <= attack_range:
        return make_action("Attack", d[0], d[1], d[2], 0, target=enemy['id'])

    # 在检测范围内 → 追击
    if dist <= detect_range:
        return make_action("Chase", d[0], d[1], d[2], move_speed * 0.9, target=enemy['id'])

    # 敌人太远 → 待命
    return make_action("Idle")


def get_ai_info():
    return {
        "name": "防御型 AI",
        "version": "1.0",
        "states": ["Idle", "Patrol", "Chase", "Attack", "Flee", "Dead"],
        "description": "守卫固定位置，限距追击，低血量撤退"
    }
