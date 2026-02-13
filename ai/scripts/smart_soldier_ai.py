"""
🔫 智能士兵 AI — 命令执行 + 本地自主

职责:
- 优先执行队长下发的命令（保持阵型/攻击目标/移动到位置）
- 在紧急情况下可以覆盖命令（自我保护优先）
- 向上级报告状态

决策优先级:
1. 自我保护（极低血量逃跑）
2. 紧急威胁（近距离敌人自动反击）
3. 执行队长命令
4. 无命令时独立巡逻/搜索

与 default_ai 的区别:
- 会接收并执行命令
- 有阵型意识
- 有队友协作行为
"""
from ai_utils import *


# ── 士兵个体记忆 ──────────────────────────────────────

soldier_memory = {}


def get_memory(entity_id):
    """获取/初始化该士兵的个体记忆"""
    if entity_id not in soldier_memory:
        soldier_memory[entity_id] = {
            "formation_index": -1,        # 阵型中的编号
            "last_damage_time": 0,         # 上次受伤时间
            "consecutive_hits": 0,         # 连续受击次数
            "known_enemy_positions": [],   # 已知敌人位置列表
            "stuck_timer": 0,              # 卡住计时器
            "last_pos": None,              # 上一帧位置
        }
    return soldier_memory[entity_id]


def update_ai(ctx_json):
    """士兵 AI 决策入口"""
    ctx = parse_context(ctx_json)

    entity_id = ctx.get("entity_id", 0)
    pos = ctx.get("pos", [0, 0, 0])
    health = ctx.get("health", 100)
    max_health = ctx.get("max_health", 100)
    enemies = ctx.get("enemies", [])
    allies = ctx.get("allies", [])
    attack_range = ctx.get("attack_range", 2)
    move_speed = ctx.get("move_speed", 3)
    dt = ctx.get("dt", 0.016)
    state = ctx.get("state", "Idle")

    mem = get_memory(entity_id)

    # 更新卡住检测
    if mem["last_pos"]:
        if distance(pos, mem["last_pos"]) < 0.01:
            mem["stuck_timer"] += dt
        else:
            mem["stuck_timer"] = 0
    mem["last_pos"] = pos[:]

    # ═══════════════════════════════════════════════════
    # 优先级 1: 自我保护（极低血量逃跑）
    # ═══════════════════════════════════════════════════

    if health <= 0:
        return make_action("Dead")

    if health < max_health * 0.15:
        if enemies:
            away = direction_to(enemies[0]['pos'], pos)
            return make_action("Flee", away[0], away[1], away[2],
                               move_speed * 1.5, custom="emergency_flee")

        # 向最近队友靠拢
        if allies:
            closest_ally = min(allies, key=lambda a: a.get("dist", 999))
            d = direction_to(pos, closest_ally['pos'])
            return make_action("Flee", d[0], d[1], d[2],
                               move_speed * 1.2, custom="retreat_to_ally")

        return make_action("Flee", 0, 0, 1, move_speed * 1.3)

    # ═══════════════════════════════════════════════════
    # 优先级 2: 紧急威胁（极近距离敌人自动反击）
    # ═══════════════════════════════════════════════════

    if enemies and enemies[0]['dist'] < attack_range * 0.8:
        threat = enemies[0]
        d = direction_to(pos, threat['pos'])
        return make_action("Attack", d[0], d[1], d[2], 0,
                           target=threat['id'], custom="self_defense")

    # ═══════════════════════════════════════════════════
    # 优先级 3: 执行队长命令
    # ═══════════════════════════════════════════════════

    order = parse_order(ctx)
    if order is not None:
        return execute_order(ctx, pos, order, enemies, allies,
                            attack_range, move_speed, mem, dt)

    # ═══════════════════════════════════════════════════
    # 优先级 4: 无命令时独立决策
    # ═══════════════════════════════════════════════════

    return independent_behavior(ctx, pos, enemies, allies,
                                attack_range, move_speed, mem, state)


# ═══════════════════════════════════════════════════════════
# 命令执行
# ═══════════════════════════════════════════════════════════

def execute_order(ctx, pos, order, enemies, allies, atk_range, speed, mem, dt):
    """根据队长命令执行行为"""
    order_type = order.get("type", "hold")
    target_pos = order.get("target_pos", [0, 0, 0])
    target_id = order.get("target_id", 0)
    formation = order.get("formation", "triangle")
    extra = order.get("extra", {})

    # ── 撤退命令 ──────────────────────────────────

    if order_type == "retreat":
        d = direction_to(pos, target_pos)
        return make_action("Flee", d[0], d[1], d[2],
                           speed * 1.3, custom="ordered_retreat")

    # ── 攻击命令 ──────────────────────────────────

    if order_type == "attack":
        if target_id and enemies:
            # 找到指定目标
            target = next((e for e in enemies if e['id'] == target_id), None)
            if target:
                d = direction_to(pos, target['pos'])
                if target['dist'] <= atk_range:
                    return make_action("Attack", d[0], d[1], d[2], 0,
                                       target=target['id'], custom="ordered_attack")
                return make_action("Chase", d[0], d[1], d[2], speed,
                                   target=target['id'], custom="ordered_chase")

        # 没找到指定目标，攻击最近敌人
        if enemies:
            target = enemies[0]
            d = direction_to(pos, target['pos'])
            if target['dist'] <= atk_range:
                return make_action("Attack", d[0], d[1], d[2], 0,
                                   target=target['id'], custom="attack_nearest")
            return make_action("Chase", d[0], d[1], d[2], speed,
                               custom="chase_nearest")

        # 无敌人，向目标点移动
        d = direction_to(pos, target_pos)
        return make_action("Patrol", d[0], d[1], d[2], speed, custom="move_to_attack")

    # ── 推进命令 ──────────────────────────────────

    if order_type == "advance":
        leader_pos = extra.get("leader_pos", target_pos)

        # 确定自己在阵型中的编号
        if mem["formation_index"] < 0:
            mem["formation_index"] = assign_formation_index(ctx, allies)

        # 计算阵型位置
        to_target = direction_to(leader_pos, target_pos)
        form_pos = formation_position(leader_pos, to_target,
                                       mem["formation_index"], formation)

        dist_to_form = distance_xz(pos, form_pos)
        if dist_to_form > 1.5:
            d = direction_to(pos, form_pos)
            return make_action("Patrol", d[0], d[1], d[2],
                               speed * 0.9, custom="formation_move")

        # 到位，跟随推进
        d = direction_to(pos, target_pos)

        # 途中遇敌反击
        if enemies and enemies[0]['dist'] <= atk_range * 1.5:
            d = direction_to(pos, enemies[0]['pos'])
            if enemies[0]['dist'] <= atk_range:
                return make_action("Attack", d[0], d[1], d[2], 0,
                                   target=enemies[0]['id'])
            return make_action("Chase", d[0], d[1], d[2], speed * 0.8)

        return make_action("Patrol", d[0], d[1], d[2], speed * 0.7,
                           custom="advancing")

    # ── 保持阵型命令 ──────────────────────────────

    if order_type == "hold_formation" or order_type == "move_to":
        leader_pos = extra.get("leader_pos", target_pos)

        if mem["formation_index"] < 0:
            mem["formation_index"] = assign_formation_index(ctx, allies)

        to_target = direction_to(leader_pos, target_pos)
        form_pos = formation_position(leader_pos, to_target,
                                       mem["formation_index"], formation)

        dist_to_form = distance_xz(pos, form_pos)
        if dist_to_form > 1.0:
            d = direction_to(pos, form_pos)
            move_speed = min(speed, dist_to_form * 2)  # 离得近慢下来
            return make_action("Patrol", d[0], d[1], d[2], move_speed,
                               custom="maintaining_formation")

        # 到位，面向前方
        if enemies and enemies[0]['dist'] <= atk_range:
            d = direction_to(pos, enemies[0]['pos'])
            return make_action("Attack", d[0], d[1], d[2], 0,
                               target=enemies[0]['id'])

        return make_action("Idle", custom="in_formation")

    # 未知命令类型 → 向目标位置移动
    d = direction_to(pos, target_pos)
    return make_action("Patrol", d[0], d[1], d[2], speed * 0.7,
                       custom="following_order")


# ═══════════════════════════════════════════════════════════
# 独立行为（无命令时）
# ═══════════════════════════════════════════════════════════

def independent_behavior(ctx, pos, enemies, allies, atk_range, speed, mem, state):
    """没有队长命令时的自主行为"""
    health = ctx.get("health", 100)
    max_health = ctx.get("max_health", 100)

    # 低血量逃跑
    if health < max_health * 0.25:
        if enemies:
            away = direction_to(enemies[0]['pos'], pos)
            return make_action("Flee", away[0], away[1], away[2],
                               speed * 1.2, custom="low_hp_flee")

    # 有敌人 → 协作战斗（优先攻击队友在攻击的目标）
    if enemies:
        # 找队友正在攻击的目标
        shared_target = find_shared_target(enemies, allies)
        if shared_target:
            d = direction_to(pos, shared_target['pos'])
            if shared_target['dist'] <= atk_range:
                return make_action("Attack", d[0], d[1], d[2], 0,
                                   target=shared_target['id'], custom="focus_fire")
            return make_action("Chase", d[0], d[1], d[2], speed,
                               custom="chase_shared_target")

        # 没有共同目标，攻击最近敌人
        target = enemies[0]
        d = direction_to(pos, target['pos'])
        if target['dist'] <= atk_range:
            return make_action("Attack", d[0], d[1], d[2], 0,
                               target=target['id'])
        return make_action("Chase", d[0], d[1], d[2], speed)

    # 卡住 → 随机移动
    if mem["stuck_timer"] > 2.0:
        import random
        angle = random.uniform(0, 360)
        d = rotate_y([0, 0, 1], angle)
        mem["stuck_timer"] = 0
        return make_action("Patrol", d[0], d[1], d[2], speed * 0.5,
                           custom="unstuck")

    # 无事可做 → 跟队友靠拢
    if allies:
        closest = min(allies, key=lambda a: a.get("dist", 999))
        if closest.get("dist", 0) > 5.0:
            d = direction_to(pos, closest['pos'])
            return make_action("Patrol", d[0], d[1], d[2], speed * 0.5,
                               custom="regroup")

    return make_action("Idle", custom="standby")


# ═══════════════════════════════════════════════════════════
# 辅助函数
# ═══════════════════════════════════════════════════════════

def assign_formation_index(ctx, allies):
    """根据实体ID在队伍中的排序确定阵型编号"""
    entity_id = ctx.get("entity_id", 0)
    all_ids = sorted([a.get("id", 0) for a in allies] + [entity_id])
    try:
        return all_ids.index(entity_id)
    except ValueError:
        return 0


def find_shared_target(enemies, allies):
    """找队友正在攻击的敌人（集火目标）"""
    # 通过检查哪个敌人周围队友最多来推断
    if not enemies or not allies:
        return None

    best_target = None
    max_allies_near = 0

    for enemy in enemies:
        allies_near = sum(1 for a in allies
                          if distance(a.get('pos', [0,0,0]), enemy['pos']) < 5.0
                          and a.get('state') in ('Attack', 'Chase'))
        if allies_near > max_allies_near:
            max_allies_near = allies_near
            best_target = enemy

    return best_target if max_allies_near > 0 else None


def get_ai_info():
    return {
        "name": "智能士兵 AI",
        "version": "1.0",
        "role": "soldier",
        "description": "执行队长命令 + 阵型保持 + 本地自主决策 + 紧急自我保护"
    }
