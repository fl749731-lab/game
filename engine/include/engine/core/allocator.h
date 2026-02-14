#pragma once

#include "engine/core/types.h"
#include "engine/core/log.h"

#include <cstdlib>
#include <cstring>
#include <vector>
#include <new>

namespace Engine {

// ── FrameAllocator ──────────────────────────────────────────
// 线性帧分配器 — 每帧开始 Reset() 一次
// O(1) 分配, 零碎片, 极致缓存友好
// 用途: 临时逐帧数据 (排序键、剔除列表、命令缓冲区)

class FrameAllocator {
public:
    static void Init(size_t capacityBytes = 4 * 1024 * 1024) {
        s_Buffer = (u8*)std::malloc(capacityBytes);
        if (!s_Buffer) {
            LOG_ERROR("[FrameAllocator] 分配 %zu 字节失败!", capacityBytes);
            return;
        }
        s_Capacity = capacityBytes;
        s_Offset = 0;
        s_PeakUsage = 0;
        LOG_INFO("[FrameAllocator] 初始化: %zu KB", capacityBytes / 1024);
    }

    static void Shutdown() {
        if (s_Buffer) {
            LOG_INFO("[FrameAllocator] 关闭 | 峰值使用: %zu / %zu KB",
                     s_PeakUsage / 1024, s_Capacity / 1024);
            std::free(s_Buffer);
            s_Buffer = nullptr;
        }
    }

    /// 分配 (O(1), bump pointer)
    static void* Alloc(size_t bytes, size_t align = 16) {
        // 对齐
        size_t aligned = (s_Offset + align - 1) & ~(align - 1);
        if (aligned + bytes > s_Capacity) {
            LOG_WARN("[FrameAllocator] 容量不足! 需要 %zu, 剩余 %zu",
                     bytes, s_Capacity - aligned);
            return nullptr;
        }
        void* ptr = s_Buffer + aligned;
        s_Offset = aligned + bytes;
        if (s_Offset > s_PeakUsage) s_PeakUsage = s_Offset;
        return ptr;
    }

    /// 类型化分配 (构造 N 个 T)
    template<typename T>
    static T* AllocArray(size_t count) {
        void* raw = Alloc(sizeof(T) * count, alignof(T));
        if (!raw) return nullptr;
        // 可以在上面 placement new 如果 T 需要构造
        return static_cast<T*>(raw);
    }

    /// 每帧开始调用 — 重置偏移量 (O(1))
    static void Reset() {
        s_Offset = 0;
    }

    /// 统计
    static size_t GetUsed() { return s_Offset; }
    static size_t GetCapacity() { return s_Capacity; }
    static size_t GetPeakUsage() { return s_PeakUsage; }

private:
    inline static u8*    s_Buffer   = nullptr;
    inline static size_t s_Capacity = 0;
    inline static size_t s_Offset   = 0;
    inline static size_t s_PeakUsage = 0;
};

// ── PoolAllocator<T> ────────────────────────────────────────
// 固定大小对象池 — 高频创建/销毁场景
// O(1) 分配和回收, free list
// 用途: 组件、粒子、碰撞对、渲染命令

template<typename T, u32 BlockSize = 1024>
class PoolAllocator {
public:
    PoolAllocator() = default;
    ~PoolAllocator() { Reset(); }

    // 禁止拷贝
    PoolAllocator(const PoolAllocator&) = delete;
    PoolAllocator& operator=(const PoolAllocator&) = delete;

    /// 分配一个 T (O(1))
    T* Alloc() {
        if (m_FreeList.empty()) {
            AllocBlock();
        }
        T* ptr = m_FreeList.back();
        m_FreeList.pop_back();
        m_AllocCount++;
        return ptr;
    }

    /// 回收一个 T (O(1))
    void Free(T* ptr) {
        if (!ptr) return;
        ptr->~T();  // 显式析构
        m_FreeList.push_back(ptr);
        m_AllocCount--;
    }

    /// 释放所有内存
    void Reset() {
        for (auto* block : m_Blocks) {
            std::free(block);
        }
        m_Blocks.clear();
        m_FreeList.clear();
        m_AllocCount = 0;
    }

    u32 GetAllocCount() const { return m_AllocCount; }
    u32 GetTotalCapacity() const { return (u32)(m_Blocks.size() * BlockSize); }

private:
    struct alignas(T) Slot {
        u8 data[sizeof(T)];
    };

    void AllocBlock() {
        Slot* block = (Slot*)std::malloc(sizeof(Slot) * BlockSize);
        if (!block) {
            LOG_ERROR("[PoolAllocator<%s>] 分配块失败!", typeid(T).name());
            return;
        }
        m_Blocks.push_back(block);
        // 将新块的所有槽位加入 free list
        for (u32 i = 0; i < BlockSize; i++) {
            m_FreeList.push_back(reinterpret_cast<T*>(&block[i]));
        }
    }

    std::vector<Slot*> m_Blocks;
    std::vector<T*> m_FreeList;
    u32 m_AllocCount = 0;
};

} // namespace Engine
