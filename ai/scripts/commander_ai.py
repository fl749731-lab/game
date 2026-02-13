"""
🧠 指挥官 AI — 全局战术决策 + 玩家意图记忆

职责:
- 分析战场全局态势（各小队状态、玩家位置/行为）
- 识别并记忆玩家策略模式（攻击倾向、走位习惯、常用战术）
- 制定战术计划，下发命令给各小队长

记忆系统:
- intent_history: 最近 100 次玩家意图分类
- player_profile: 玩家风格画像（累积更新）
- tactical_history: 战术执行效果记录
"""
from collections import deque
from ai_utils import *

# ═══════════════════════════════════════════════════════════
# 持久记忆（跨帧保持，Python 进程内存中）
# ═══════════════════════════════════════════════════════════

player_memory = {
    # 短期记忆：最近 100 次意图分类
    "intent_history": deque(maxlen=100),

    # 长期画像
    "aggression_score": 0.5,      # 攻击倾向 0~1（0=被动 1=激进）
    "preferred_approach": "direct", # "direct"|"flanking"|"kiting"|"camping"
    "avg_engagement_range": 5.0,  # 平均交战距离
    "retreat_threshold": 0.3,     # 玩家通常在多少血比时撤退

    # 战术效果记录
    "tactic_results": {
        "flank_left":  {"used": 0, "success": 0},
        "flank_right": {"used": 0, "success": 0},
        "pincer":      {"used": 0, "success": 0},
        "rush":        {"used": 0, "success": 0},
        "defend":      {"used": 0, "success": 0},
        "retreat":     {"used": 0, "success": 0},
    },

    # 上次战术
    "last_tactic": "defend",
    "tactic_timer": 0.0,
    "tactic_cooldown": 5.0,  # 最少 5 秒换一次战术
}


# ═══════════════════════════════════════════════════════════
# 玩家意图分类
# ═══════════════════════════════════════════════════════════

def classify_player_intent(player):
    """根据玩家行为数据分类当前意图"""
    if player is None:
        return "unknown"

    speed = player.get("speed", 0)
    avg_speed = player.get("avg_speed", 0)
    attacks = player.get("attack_count", 0)
    retreats = player.get("retreat_count", 0)
    aggression = player.get("aggression", 0.5)

    # 激进冲锋：高速 + 高攻击频率
    if speed > 5.0 and attacks > 3:
        return "aggressive_rush"

    # 风筝走位：高速 + 频繁后退 + 有攻击
    if speed > 3.0 and retreats > 2 and attacks > 1:
        return "kiting"

    # 蹲点防守：低速 + 有攻击
    if speed < 1.0 and attacks > 0:
        return "camping"

    # 谨慎推进：中速 + 低攻击
    if 1.0 <= speed <= 4.0 and attacks <= 2:
        return "cautious_advance"

    # 逃跑：高速 + 高后退 + 低攻击
    if speed > 4.0 and retreats > attacks:
        return "fleeing"

    # 探索：中速 + 无攻击
    if speed > 0.5 and attacks == 0:
        return "exploring"

    return "idle"


def update_player_profile(player, intent):
    """更新玩家长期画像"""
    mem = player_memory

    # 记录意图
    mem["intent_history"].append(intent)

    # 更新攻击倾向（指数移动平均）
    if player:
        new_aggression = player.get("aggression", 0.5)
        mem["aggression_score"] = mem["aggression_score"] * 0.9 + new_aggression * 0.1

    # 识别偏好接近方式
    history = list(mem["intent_history"])
    if len(history) >= 10:
        recent = history[-20:]
        rush_count = recent.count("aggressive_rush")
        kite_count = recent.count("kiting")
        camp_count = recent.count("camping")
        cautious_count = recent.count("cautious_advance")

        max_count = max(rush_count, kite_count, camp_count, cautious_count)
        if max_count == rush_count:
            mem["preferred_approach"] = "direct"
        elif max_count == kite_count:
            mem["preferred_approach"] = "kiting"
        elif max_count == camp_count:
            mem["preferred_approach"] = "camping"
        else:
            mem["preferred_approach"] = "cautious"


# ═══════════════════════════════════════════════════════════
# 战术选择
# ═══════════════════════════════════════════════════════════

def choose_tactic(ctx):
    """根据态势和玩家画像选择最优战术"""
    mem = player_memory
    squads = ctx.get("squads", [])
    player = ctx.get("player")
    pos = ctx.get("pos", [0, 0, 0])
    intent = classify_player_intent(player)

    update_player_profile(player, intent)

    # 检查冷却
    mem["tactic_timer"] += ctx.get("dt", 0.016)
    if mem["tactic_timer"] < mem["tactic_cooldown"]:
        return mem["last_tactic"]

    mem["tactic_timer"] = 0.0

    # 计算总兵力
    total_alive = sum(s.get("alive", 0) for s in squads)
    total_members = sum(s.get("total", 0) for s in squads)
    force_ratio = total_alive / max(total_members, 1)

    # ── 战术决策矩阵 ──────────────────────────────────

    # 兵力不足 → 防御/撤退
    if force_ratio < 0.3:
        tactic = "retreat"
    elif force_ratio < 0.5:
        tactic = "defend"

    # 玩家激进冲锋 → 反手包夹
    elif intent == "aggressive_rush":
        tactic = "pincer"  # 两侧夹击

    # 玩家风筝 → 多路包抄切断退路
    elif intent == "kiting":
        # 选历史成功率更高的侧翼
        left = mem["tactic_results"]["flank_left"]
        right = mem["tactic_results"]["flank_right"]
        left_rate = left["success"] / max(left["used"], 1)
        right_rate = right["success"] / max(right["used"], 1)
        tactic = "flank_left" if left_rate >= right_rate else "flank_right"

    # 玩家蹲点 → 正面牵制 + 绕后
    elif intent == "camping":
        tactic = "flank_right"  # 绕后

    # 玩家逃跑 → 全力追击
    elif intent == "fleeing":
        tactic = "rush"

    # 默认：正面推进
    else:
        if mem["aggression_score"] > 0.6:
            tactic = "pincer"
        else:
            tactic = "rush"

    # 记录使用
    if tactic in mem["tactic_results"]:
        mem["tactic_results"][tactic]["used"] += 1

    mem["last_tactic"] = tactic
    return tactic


# ═══════════════════════════════════════════════════════════
# 主入口
# ═══════════════════════════════════════════════════════════

def update_ai(ctx_json):
    """指挥官 AI 决策入口"""
    ctx = parse_context(ctx_json)

    pos = ctx.get("pos", [0, 0, 0])
    health = ctx.get("health", 100)
    max_health = ctx.get("max_health", 100)
    player = ctx.get("player")
    squads = ctx.get("squads", [])

    # 死亡
    if health <= 0:
        return make_action("Dead")

    # 选择战术
    tactic = choose_tactic(ctx)

    # 构建命令
    target_pos = player["pos"] if player else [0, 0, 0]

    if tactic == "retreat":
        # 所有小队撤退
        order = make_order("retreat", target_pos=pos, priority=1.0, formation="line")

    elif tactic == "defend":
        order = make_order("defend", target_pos=pos, priority=0.8, formation="line")

    elif tactic == "pincer":
        # 钳形攻击：让小队从两侧包夹
        # 不同小队收到不同偏移，通过 extra 字段区分
        order = make_order("pincer", target_pos=target_pos, priority=0.9,
                          formation="wedge",
                          extra={"left_squad": True, "spread_angle": 60})

    elif tactic == "flank_left":
        order = make_order("flank", target_pos=target_pos, priority=0.7,
                          formation="triangle",
                          extra={"side": "left", "offset": 15.0})

    elif tactic == "flank_right":
        order = make_order("flank", target_pos=target_pos, priority=0.7,
                          formation="triangle",
                          extra={"side": "right", "offset": 15.0})

    elif tactic == "rush":
        order = make_order("attack", target_pos=target_pos, priority=0.9,
                          formation="wedge")

    else:
        order = make_order("hold", target_pos=pos, priority=0.3, formation="spread")

    # 指挥官自身不移动，在后方指挥
    return make_action("Idle", custom=f"tactic:{tactic}", order=order)


def get_ai_info():
    return {
        "name": "指挥官 AI",
        "version": "1.0",
        "role": "commander",
        "description": "全局战术决策 + 玩家意图识别 + 策略记忆，下发命令给小队长"
    }
