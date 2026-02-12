"""
AI 工具库 — 公共函数
供所有 AI 脚本使用
"""
import math
import json


def distance(a, b):
    """欧几里得距离 (3D)"""
    dx = a[0] - b[0]
    dy = a[1] - b[1]
    dz = a[2] - b[2]
    return math.sqrt(dx*dx + dy*dy + dz*dz)


def distance_xz(a, b):
    """水平距离 (忽略 Y 轴)"""
    dx = a[0] - b[0]
    dz = a[2] - b[2]
    return math.sqrt(dx*dx + dz*dz)


def normalize(vec):
    """归一化 3D 向量"""
    length = math.sqrt(vec[0]**2 + vec[1]**2 + vec[2]**2)
    if length < 0.0001:
        return [0, 0, 0]
    return [vec[0]/length, vec[1]/length, vec[2]/length]


def direction_to(src, dst):
    """从 src 指向 dst 的方向向量 (归一化)"""
    diff = [dst[0]-src[0], dst[1]-src[1], dst[2]-src[2]]
    return normalize(diff)


def lerp(a, b, t):
    """线性插值"""
    return a + (b - a) * t


def clamp(value, min_val, max_val):
    """限制范围"""
    return max(min_val, min(value, max_val))


def choose_closest_enemy(ctx):
    """选择最近的敌人 (enemies 已按距离排序)"""
    enemies = ctx.get('enemies', [])
    if not enemies:
        return None
    return enemies[0]


def choose_weakest_enemy(ctx):
    """选择血量最低的敌人"""
    enemies = ctx.get('enemies', [])
    if not enemies:
        return None
    return min(enemies, key=lambda e: e['health'])


def make_action(state, dir_x=0, dir_y=0, dir_z=0, speed=0, target=0, custom=""):
    """构建返回给 C++ 的动作字符串
    格式: state|dir_x,dir_y,dir_z|speed|target_id|custom
    """
    return f"{state}|{dir_x:.3f},{dir_y:.3f},{dir_z:.3f}|{speed:.2f}|{target}|{custom}"


def parse_context(ctx_json):
    """解析 C++ 传来的 JSON 上下文"""
    try:
        return json.loads(ctx_json)
    except Exception:
        return {
            'entity_id': 0, 'pos': [0,0,0], 'health': 100,
            'max_health': 100, 'state': 'Idle', 'dt': 0.016,
            'enemies': [], 'patrol_points': [], 'patrol_index': 0,
            'detect_range': 10, 'attack_range': 2, 'move_speed': 3
        }
