#pragma once

#include "engine/rhi/rhi_texture.h"

#include <glad/glad.h>
#include <string>

namespace Engine {

// ── OpenGL 2D 纹理 ─────────────────────────────────────────

class GLTexture2D : public RHITexture2D {
public:
    /// 从文件加载
    GLTexture2D(const std::string& filepath);
    /// 创建空纹理 (可选初始数据)
    GLTexture2D(u32 width, u32 height, const void* data = nullptr);
    ~GLTexture2D() override;

    GLTexture2D(const GLTexture2D&) = delete;
    GLTexture2D& operator=(const GLTexture2D&) = delete;

    void Bind(u32 slot = 0) const override;
    void Unbind() const override;
    void SetData(const void* data, u32 size) override;

    u32  GetWidth() const override  { return m_Width; }
    u32  GetHeight() const override { return m_Height; }
    bool IsValid() const override   { return m_ID != 0; }

    u32 GetID() const { return m_ID; }

private:
    u32 m_ID             = 0;
    u32 m_Width          = 0;
    u32 m_Height         = 0;
    u32 m_Channels       = 0;
    u32 m_InternalFormat = 0;
    u32 m_DataFormat     = 0;
};

} // namespace Engine
