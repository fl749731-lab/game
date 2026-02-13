"""
🎖 小队长 AI — 小队战术协调

职责:
- 接收指挥官命令，分解为个体子命令
- 管理阵型（4种：三角/一字/散开/楔形）
- 协调队员分工（突击/掩护/侧翼）
- 根据战场实况微调命令

输入: 指挥官命令 (order) + 队友信息 (allies) + 敌人信息 (enemies) + 玩家数据 (player)
输出: 自身行动 + 下发给士兵的子命令
"""
from ai_utils import *


def update_ai(ctx_json):
    """小队长 AI 决策入口"""
    ctx = parse_context(ctx_json)

    pos = ctx.get("pos", [0, 0, 0])
    health = ctx.get("health", 100)
    max_health = ctx.get("max_health", 100)
    enemies = ctx.get("enemies", [])
    allies = ctx.get("allies", [])
    player = ctx.get("player")
    attack_range = ctx.get("attack_range", 2)
    move_speed = ctx.get("move_speed", 3)

    # 死亡
    if health <= 0:
        return make_action("Dead")

    # 解析指挥官命令
    order = parse_order(ctx)

    # ── 没有命令时：独立决策 ──────────────────────

    if order is None:
        return independent_decision(ctx, pos, enemies, allies, move_speed, attack_range)

    order_type = order.get("type", "hold")
    target_pos = order.get("target_pos", [0, 0, 0])
    formation = order.get("formation", "triangle")
    extra = order.get("extra", {})

    # ── 执行指挥官命令 ────────────────────────────

    if order_type == "retreat":
        return execute_retreat(ctx, pos, target_pos, formation, allies, move_speed)

    elif order_type == "defend":
        return execute_defend(ctx, pos, target_pos, formation, enemies, allies, move_speed, attack_range)

    elif order_type == "attack" or order_type == "rush":
        return execute_attack(ctx, pos, target_pos, formation, enemies, allies, move_speed, attack_range)

    elif order_type == "pincer":
        return execute_pincer(ctx, pos, target_pos, formation, extra, enemies, allies, move_speed, attack_range)

    elif order_type == "flank":
        return execute_flank(ctx, pos, target_pos, formation, extra, enemies, allies, move_speed, attack_range)

    elif order_type == "hold":
        return execute_hold(ctx, pos, formation, enemies, allies, move_speed, attack_range)

    # 默认
    return independent_decision(ctx, pos, enemies, allies, move_speed, attack_range)


# ═══════════════════════════════════════════════════════════
# 战术执行
# ═══════════════════════════════════════════════════════════

def execute_retreat(ctx, pos, rally_point, formation, allies, speed):
    """撤退: 全队向集结点撤退"""
    d = direction_to(pos, rally_point)
    # 命令士兵也撤退
    soldier_order = make_order("retreat", target_pos=rally_point,
                               priority=1.0, formation="line")
    return make_action("Flee", d[0], d[1], d[2], speed * 1.2,
                       custom="retreat", order=soldier_order)


def execute_defend(ctx, pos, defend_pos, formation, enemies, allies, speed, atk_range):
    """防御: 在指定位置结阵防守"""
    dist_to_pos = distance_xz(pos, defend_pos)

    if dist_to_pos > 3.0:
        # 还没到防守位置，先跑过去
        d = direction_to(pos, defend_pos)
        soldier_order = make_order("move_to", target_pos=defend_pos,
                                   formation=formation, priority=0.8)
        return make_action("Patrol", d[0], d[1], d[2], speed,
                           custom="moving_to_defend", order=soldier_order)

    # 到位了，命令士兵保持阵型
    soldier_order = make_order("hold_formation", target_pos=defend_pos,
                               formation=formation, priority=0.7,
                               extra={"leader_pos": pos})
    if enemies:
        enemy = enemies[0]
        if enemy['dist'] <= atk_range:
            d = direction_to(pos, enemy['pos'])
            return make_action("Attack", d[0], d[1], d[2], 0,
                               target=enemy['id'], order=soldier_order)

    return make_action("Idle", custom="defending", order=soldier_order)


def execute_attack(ctx, pos, target_pos, formation, enemies, allies, speed, atk_range):
    """进攻: 全队以阵型向目标推进"""
    # 队长带头冲
    if enemies:
        target = choose_priority_target(ctx)
        if target and target['dist'] <= atk_range:
            d = direction_to(pos, target['pos'])
            soldier_order = make_order("attack", target_pos=target['pos'],
                                       target_id=target['id'],
                                       formation=formation, priority=0.9)
            return make_action("Attack", d[0], d[1], d[2], 0,
                               target=target['id'], order=soldier_order)

    # 向目标位置推进
    d = direction_to(pos, target_pos)
    soldier_order = make_order("advance", target_pos=target_pos,
                               formation=formation, priority=0.8,
                               extra={"leader_pos": pos})
    return make_action("Chase", d[0], d[1], d[2], speed,
                       custom="advancing", order=soldier_order)


def execute_pincer(ctx, pos, target_pos, formation, extra, enemies, allies, speed, atk_range):
    """钳形攻击: 绕到目标两侧夹击"""
    spread_angle = extra.get("spread_angle", 60)
    to_target = direction_to(pos, target_pos)

    # 根据小队ID决定走左还是右
    squad_id = ctx.get("squad_id", 0)
    if squad_id % 2 == 0:
        flank_dir = rotate_y(to_target, spread_angle)
        side = "left"
    else:
        flank_dir = rotate_y(to_target, -spread_angle)
        side = "right"

    # 计算侧翼攻击点
    flank_pos = vec_add(target_pos, vec_scale(flank_dir, -8.0))

    dist_to_flank = distance_xz(pos, flank_pos)
    if dist_to_flank > 3.0:
        d = direction_to(pos, flank_pos)
        soldier_order = make_order("move_to", target_pos=flank_pos,
                                   formation="wedge", priority=0.9,
                                   extra={"leader_pos": pos, "side": side})
        return make_action("Chase", d[0], d[1], d[2], speed,
                           custom=f"flanking_{side}", order=soldier_order)

    # 到达侧翼位置，攻击！
    soldier_order = make_order("attack", target_pos=target_pos,
                               formation="wedge", priority=1.0)
    if enemies:
        target = enemies[0]
        d = direction_to(pos, target['pos'])
        return make_action("Attack", d[0], d[1], d[2], speed * 0.8,
                           target=target['id'], order=soldier_order)

    d = direction_to(pos, target_pos)
    return make_action("Chase", d[0], d[1], d[2], speed, order=soldier_order)


def execute_flank(ctx, pos, target_pos, formation, extra, enemies, allies, speed, atk_range):
    """侧翼机动: 从指定侧面绕行攻击"""
    side = extra.get("side", "right")
    offset = extra.get("offset", 15.0)

    to_target = direction_to(pos, target_pos)
    angle = -90 if side == "right" else 90
    flank_dir = rotate_y(to_target, angle)
    flank_pos = vec_add(target_pos, vec_scale(flank_dir, offset))

    dist = distance_xz(pos, flank_pos)
    if dist > 3.0:
        d = direction_to(pos, flank_pos)
        soldier_order = make_order("move_to", target_pos=flank_pos,
                                   formation=formation, priority=0.8,
                                   extra={"leader_pos": pos, "side": side})
        return make_action("Patrol", d[0], d[1], d[2], speed * 0.8,
                           custom=f"flanking_{side}", order=soldier_order)

    # 到侧翼位置，转向目标攻击
    d = direction_to(pos, target_pos)
    soldier_order = make_order("attack", target_pos=target_pos,
                               formation="wedge", priority=1.0)
    return make_action("Chase", d[0], d[1], d[2], speed,
                       custom="flank_attack", order=soldier_order)


def execute_hold(ctx, pos, formation, enemies, allies, speed, atk_range):
    """原地驻守"""
    soldier_order = make_order("hold_formation", target_pos=pos,
                               formation=formation, priority=0.5,
                               extra={"leader_pos": pos})
    if enemies and enemies[0]['dist'] <= atk_range * 2:
        target = enemies[0]
        d = direction_to(pos, target['pos'])
        if target['dist'] <= atk_range:
            return make_action("Attack", d[0], d[1], d[2], 0,
                               target=target['id'], order=soldier_order)
        return make_action("Chase", d[0], d[1], d[2], speed * 0.5,
                           order=soldier_order)

    return make_action("Idle", custom="holding", order=soldier_order)


def independent_decision(ctx, pos, enemies, allies, speed, atk_range):
    """无命令时的独立决策"""
    health = ctx.get("health", 100)
    max_health = ctx.get("max_health", 100)

    # 低血量撤退
    if health < max_health * 0.2:
        if enemies:
            away = direction_to(enemies[0]['pos'], pos)
            soldier_order = make_order("retreat", target_pos=pos, priority=1.0)
            return make_action("Flee", away[0], away[1], away[2], speed * 1.3,
                               custom="emergency_retreat", order=soldier_order)

    # 有敌人 → 攻击
    if enemies:
        target = choose_priority_target(ctx)
        if target:
            d = direction_to(pos, target['pos'])
            if target['dist'] <= atk_range:
                soldier_order = make_order("attack", target_pos=target['pos'],
                                           target_id=target['id'], priority=0.8)
                return make_action("Attack", d[0], d[1], d[2], 0,
                                   target=target['id'], order=soldier_order)
            soldier_order = make_order("advance", target_pos=target['pos'],
                                       formation="triangle", priority=0.6,
                                       extra={"leader_pos": pos})
            return make_action("Chase", d[0], d[1], d[2], speed,
                               order=soldier_order)

    return make_action("Idle")


def get_ai_info():
    return {
        "name": "小队长 AI",
        "version": "1.0",
        "role": "leader",
        "description": "接收指挥官命令，管理阵型，协调队员，执行战术"
    }
