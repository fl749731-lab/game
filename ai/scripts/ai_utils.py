"""
AI 工具库 — 公共函数
供所有 AI 脚本使用（含小队/指挥链工具）
"""
import math
import json


# ════════════════════════════════════════════════════════════
# 基础数学
# ════════════════════════════════════════════════════════════

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


def vec_add(a, b):
    """向量加法"""
    return [a[0]+b[0], a[1]+b[1], a[2]+b[2]]


def vec_sub(a, b):
    """向量减法"""
    return [a[0]-b[0], a[1]-b[1], a[2]-b[2]]


def vec_scale(v, s):
    """向量缩放"""
    return [v[0]*s, v[1]*s, v[2]*s]


def vec_length(v):
    """向量长度"""
    return math.sqrt(v[0]**2 + v[1]**2 + v[2]**2)


def midpoint(a, b):
    """中点"""
    return [(a[0]+b[0])/2, (a[1]+b[1])/2, (a[2]+b[2])/2]


def rotate_y(vec, angle_deg):
    """绕 Y 轴旋转向量"""
    rad = math.radians(angle_deg)
    cos_a = math.cos(rad)
    sin_a = math.sin(rad)
    return [
        vec[0] * cos_a + vec[2] * sin_a,
        vec[1],
        -vec[0] * sin_a + vec[2] * cos_a
    ]


# ════════════════════════════════════════════════════════════
# 敌人选择
# ════════════════════════════════════════════════════════════

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


def choose_priority_target(ctx):
    """综合优先级选择目标 (距离权重40% + 血量权重60%)"""
    enemies = ctx.get('enemies', [])
    if not enemies:
        return None

    max_dist = max(e['dist'] for e in enemies) + 0.1
    max_hp = max(e['health'] for e in enemies) + 0.1

    def score(e):
        dist_score = 1.0 - (e['dist'] / max_dist)
        hp_score = 1.0 - (e['health'] / max_hp)
        return dist_score * 0.4 + hp_score * 0.6

    return max(enemies, key=score)


# ════════════════════════════════════════════════════════════
# 动作构建
# ════════════════════════════════════════════════════════════

def make_action(state, dir_x=0, dir_y=0, dir_z=0, speed=0, target=0, custom="", order=""):
    """构建返回给 C++ 的动作字符串
    格式: state|dir_x,dir_y,dir_z|speed|target_id|custom|order_json
    """
    return f"{state}|{dir_x:.3f},{dir_y:.3f},{dir_z:.3f}|{speed:.2f}|{target}|{custom}|{order}"


def make_order(order_type, target_pos=None, target_id=0, priority=0.5,
               formation="triangle", extra=None):
    """构建命令 JSON 字符串（指挥官/队长 → 下属）"""
    order = {
        "type": order_type,
        "target_pos": target_pos or [0, 0, 0],
        "target_id": target_id,
        "priority": priority,
        "formation": formation,
    }
    if extra:
        order["extra"] = extra
    return json.dumps(order)


# ════════════════════════════════════════════════════════════
# 上下文解析
# ════════════════════════════════════════════════════════════

def parse_context(ctx_json):
    """解析 C++ 传来的 JSON 上下文"""
    try:
        return json.loads(ctx_json)
    except Exception:
        return {
            'entity_id': 0, 'pos': [0,0,0], 'health': 100,
            'max_health': 100, 'state': 'Idle', 'dt': 0.016,
            'enemies': [], 'patrol_points': [], 'patrol_index': 0,
            'detect_range': 10, 'attack_range': 2, 'move_speed': 3,
            'role': 'soldier', 'squad_id': 0, 'order': None,
            'allies': [], 'player': None, 'squads': []
        }


def parse_order(ctx):
    """从上下文中提取命令"""
    order = ctx.get('order')
    if order is None:
        return None
    if isinstance(order, str):
        try:
            return json.loads(order)
        except Exception:
            return None
    return order


# ════════════════════════════════════════════════════════════
# 小队阵型计算
# ════════════════════════════════════════════════════════════

def formation_position(center, forward_dir, index, formation="triangle", spacing=2.0):
    """计算阵型中第 index 个成员的目标位置

    阵型类型:
    - triangle: 三角阵（队长在前，两翼展开）
    - line: 一字横排
    - spread: 散开（以中心为圆心扇形展开）
    - wedge: 楔形（V型前冲阵）
    """
    fwd = normalize(forward_dir) if vec_length(forward_dir) > 0.01 else [0, 0, 1]
    # 右向量（Y轴叉积）
    right = [fwd[2], 0, -fwd[0]]

    if formation == "triangle":
        offsets = [
            [0, 0, 0],                                      # 0: 队长/尖兵（前方）
            [-spacing, 0, -spacing],                         # 1: 左后
            [spacing, 0, -spacing],                          # 2: 右后
            [-spacing*2, 0, -spacing*2],                     # 3: 更左后
            [spacing*2, 0, -spacing*2],                      # 4: 更右后
        ]
    elif formation == "line":
        offset_x = (index - 2) * spacing  # 居中排列
        offsets = [[offset_x, 0, 0]] * 5
        return vec_add(center, [
            right[0] * (index - 2) * spacing,
            0,
            right[2] * (index - 2) * spacing
        ])
    elif formation == "spread":
        angle = (index * 72) - 144  # 5人扇形 -144, -72, 0, 72, 144
        dir_rotated = rotate_y(fwd, angle)
        return vec_add(center, vec_scale(dir_rotated, spacing * 1.5))
    elif formation == "wedge":
        offsets = [
            [0, 0, spacing],                                 # 0: 尖端
            [-spacing, 0, 0],                                # 1: 左翼
            [spacing, 0, 0],                                 # 2: 右翼
            [-spacing*1.5, 0, -spacing],                     # 3: 左后翼
            [spacing*1.5, 0, -spacing],                      # 4: 右后翼
        ]
    else:
        offsets = [[0, 0, 0]] * 5

    idx = min(index, len(offsets) - 1)
    off = offsets[idx]

    # 将偏移从阵型局部坐标转为世界坐标
    world_off = [
        right[0] * off[0] + fwd[0] * off[2],
        off[1],
        right[2] * off[0] + fwd[2] * off[2]
    ]
    return vec_add(center, world_off)
