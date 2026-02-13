"""
Engine API — Python 脚本调用引擎功能的接口

这是一个 Python 侧的 API 封装模块。
当 Python 启用时，引擎会自动注入 _engine_bridge 模块，
此文件提供友好的 Python API 包装。

如果 _engine_bridge 不可用（独立测试时），则使用 stub 模式。
"""
import json

# 尝试导入 C++ 注入的桥接模块
try:
    import _engine_bridge as _bridge
    _HAS_BRIDGE = True
except ImportError:
    _HAS_BRIDGE = False


# ── Transform ────────────────────────────────────────────────

def get_position(entity_id):
    """获取实体位置 → [x, y, z]"""
    if _HAS_BRIDGE:
        return _bridge.get_position(entity_id)
    return [0.0, 0.0, 0.0]

def set_position(entity_id, x, y, z):
    """设置实体位置"""
    if _HAS_BRIDGE:
        _bridge.set_position(entity_id, x, y, z)

def get_rotation(entity_id):
    """获取实体旋转 → [rx, ry, rz]"""
    if _HAS_BRIDGE:
        return _bridge.get_rotation(entity_id)
    return [0.0, 0.0, 0.0]

def set_rotation(entity_id, rx, ry, rz):
    """设置实体旋转"""
    if _HAS_BRIDGE:
        _bridge.set_rotation(entity_id, rx, ry, rz)

def get_scale(entity_id):
    """获取实体缩放 → [sx, sy, sz]"""
    if _HAS_BRIDGE:
        return _bridge.get_scale(entity_id)
    return [1.0, 1.0, 1.0]

def set_scale(entity_id, sx, sy, sz):
    """设置实体缩放"""
    if _HAS_BRIDGE:
        _bridge.set_scale(entity_id, sx, sy, sz)


# ── Physics ──────────────────────────────────────────────────

def add_force(entity_id, fx, fy, fz):
    """施加持续力 (F=ma)"""
    if _HAS_BRIDGE:
        _bridge.add_force(entity_id, fx, fy, fz)

def add_impulse(entity_id, ix, iy, iz):
    """施加瞬间冲量"""
    if _HAS_BRIDGE:
        _bridge.add_impulse(entity_id, ix, iy, iz)

def get_velocity(entity_id):
    """获取速度 → [vx, vy, vz]"""
    if _HAS_BRIDGE:
        return _bridge.get_velocity(entity_id)
    return [0.0, 0.0, 0.0]

def set_velocity(entity_id, vx, vy, vz):
    """设置速度"""
    if _HAS_BRIDGE:
        _bridge.set_velocity(entity_id, vx, vy, vz)


# ── Entity ───────────────────────────────────────────────────

def spawn_entity(name, x=0, y=0, z=0, script_module=""):
    """创建新实体，返回 entity_id"""
    if _HAS_BRIDGE:
        return _bridge.spawn_entity(name, x, y, z, script_module)
    return 0

def destroy_entity(entity_id):
    """标记实体待销毁"""
    if _HAS_BRIDGE:
        _bridge.destroy_entity(entity_id)

def find_by_tag(tag):
    """按名称查找实体列表 → [entity_id, ...]"""
    if _HAS_BRIDGE:
        return _bridge.find_by_tag(tag)
    return []

def find_nearby(entity_id, radius):
    """查找附近实体 → [{"id": int, "pos": [x,y,z], "dist": float, "name": str}, ...]"""
    if _HAS_BRIDGE:
        result = _bridge.find_nearby(entity_id, radius)
        try:
            return json.loads(result)
        except Exception:
            return []
    return []

def get_entity_count():
    """获取场景中实体总数"""
    if _HAS_BRIDGE:
        return _bridge.get_entity_count()
    return 0


# ── Component ────────────────────────────────────────────────

def get_health(entity_id):
    """获取生命值 → (current, max)"""
    if _HAS_BRIDGE:
        return _bridge.get_health(entity_id)
    return (100.0, 100.0)

def set_health(entity_id, current, max_hp=None):
    """设置生命值"""
    if _HAS_BRIDGE:
        if max_hp is None:
            _bridge.set_health(entity_id, current)
        else:
            _bridge.set_health_full(entity_id, current, max_hp)

def get_var(entity_id, key, default=0.0):
    """获取脚本浮点变量"""
    if _HAS_BRIDGE:
        return _bridge.get_float_var(entity_id, key, default)
    return default

def set_var(entity_id, key, value):
    """设置脚本浮点变量"""
    if _HAS_BRIDGE:
        _bridge.set_float_var(entity_id, key, value)

def get_string_var(entity_id, key, default=""):
    """获取脚本字符串变量"""
    if _HAS_BRIDGE:
        return _bridge.get_string_var(entity_id, key, default)
    return default

def set_string_var(entity_id, key, value):
    """设置脚本字符串变量"""
    if _HAS_BRIDGE:
        _bridge.set_string_var(entity_id, key, value)


# ── Event ────────────────────────────────────────────────────

def send_event(event_type, target_id=0, data=""):
    """发送事件到指定实体 (0=广播)"""
    if _HAS_BRIDGE:
        _bridge.send_event(event_type, target_id, data)


# ── Audio ────────────────────────────────────────────────────

def play_sound(sound_name, volume=1.0):
    """播放音效"""
    if _HAS_BRIDGE:
        _bridge.play_sound(sound_name, volume)

def stop_sound(sound_name):
    """停止音效"""
    if _HAS_BRIDGE:
        _bridge.stop_sound(sound_name)


# ── Debug ────────────────────────────────────────────────────

def log_info(msg):
    """输出 INFO 日志"""
    if _HAS_BRIDGE:
        _bridge.log_info(str(msg))
    else:
        print(f"[INFO] {msg}")

def log_warn(msg):
    """输出 WARN 日志"""
    if _HAS_BRIDGE:
        _bridge.log_warn(str(msg))
    else:
        print(f"[WARN] {msg}")

def log_error(msg):
    """输出 ERROR 日志"""
    if _HAS_BRIDGE:
        _bridge.log_error(str(msg))
    else:
        print(f"[ERROR] {msg}")


# ── Time ─────────────────────────────────────────────────────

def get_time():
    """获取引擎运行总时间 (秒)"""
    if _HAS_BRIDGE:
        return _bridge.get_time()
    return 0.0
