"""
激进型 AI — 主动攻击
- 始终搜索并攻击最近/最弱敌人
- 不巡逻，直接冲向敌人
- 低血量时仍然攻击（到 10% 才逃）
"""
from ai_utils import *


def update_ai(ctx_json):
    ctx = parse_context(ctx_json)

    pos = ctx['pos']
    health = ctx['health']
    max_health = ctx['max_health']
    enemies = ctx['enemies']
    attack_range = ctx['attack_range']
    move_speed = ctx['move_speed']

    if health <= 0:
        return make_action("Dead")

    # 仅 10% 以下才逃
    if health < max_health * 0.1:
        if enemies:
            away = direction_to(enemies[0]['pos'], pos)
            return make_action("Flee", away[0], away[1], away[2], move_speed * 1.5)
        return make_action("Flee")

    # 没有敌人 → 原地搜索
    if not enemies:
        # 缓慢旋转搜索（原地不动）
        return make_action("Idle", custom="searching")

    # 选择最弱目标
    target = choose_weakest_enemy(ctx)
    if not target:
        target = enemies[0]

    dist = distance(pos, target['pos'])
    d = direction_to(pos, target['pos'])

    # 在攻击范围内 → 攻击
    if dist <= attack_range:
        return make_action("Attack", d[0], d[1], d[2], 0, target=target['id'])

    # 冲刺追击（加速）
    return make_action("Chase", d[0], d[1], d[2], move_speed * 1.2, target=target['id'])


def get_ai_info():
    return {
        "name": "激进型 AI",
        "version": "1.0",
        "states": ["Idle", "Chase", "Attack", "Flee", "Dead"],
        "description": "主动搜索敌人，优先攻击最弱目标，极低血量才逃跑"
    }
