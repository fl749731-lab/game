"""
默认 AI 脚本 — 简单状态机
由 C++ Engine 通过 Python C API 调用
"""

def update_ai(entity_id, current_state, health, dt):
    """
    决定 AI 下一步的行为状态。
    
    参数:
        entity_id:     实体 ID (字符串)
        current_state: 当前状态 (Idle/Patrol/Chase/Attack/Flee/Dead)
        health:        当前生命值 (字符串)
        dt:            帧间隔时间 (字符串)
    
    返回:
        新状态字符串
    """
    health = float(health)
    dt = float(dt)
    entity_id = int(entity_id)
    
    # 死亡检查
    if health <= 0:
        return "Dead"
    
    # 低血量逃跑
    if health < 20:
        return "Flee"
    
    # 简单状态转换逻辑
    if current_state == "Idle":
        return "Patrol"
    elif current_state == "Patrol":
        # 每次调用有小概率发现目标
        import random
        if random.random() < 0.01:
            return "Chase"
        return "Patrol"
    elif current_state == "Chase":
        return "Attack"
    elif current_state == "Attack":
        if health < 50:
            return "Flee"
        return "Attack"
    elif current_state == "Flee":
        if health > 50:
            return "Patrol"
        return "Flee"
    
    return current_state


def get_ai_info():
    """返回 AI 系统信息"""
    return {
        "name": "默认状态机 AI",
        "version": "0.1",
        "states": ["Idle", "Patrol", "Chase", "Attack", "Flee", "Dead"]
    }
