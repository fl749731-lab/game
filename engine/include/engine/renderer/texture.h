#pragma once

#include "engine/core/types.h"
#include <string>

namespace Engine {

// ── 纹理 ────────────────────────────────────────────────────

class Texture2D {
public:
    /// 从文件加载纹理
    Texture2D(const std::string& filepath);
    /// 创建空纹理（可用于程序化填充）
    Texture2D(u32 width, u32 height, const void* data = nullptr);
    ~Texture2D();

    // 禁止拷贝
    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    void Bind(u32 slot = 0) const;
    void Unbind() const;

    /// 更新纹理数据（子区域）
    void SetData(const void* data, u32 size);

    u32 GetID() const { return m_ID; }
    u32 GetWidth() const { return m_Width; }
    u32 GetHeight() const { return m_Height; }

    bool IsValid() const { return m_ID != 0; }

private:
    u32 m_ID = 0;
    u32 m_Width = 0;
    u32 m_Height = 0;
    u32 m_Channels = 0;
    u32 m_InternalFormat = 0;
    u32 m_DataFormat = 0;
};

} // namespace Engine
