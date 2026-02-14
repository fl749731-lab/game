#pragma once

#include "engine/rhi/rhi_types.h"

namespace Engine {

// ── 抽象渲染管线状态 ────────────────────────────────────────
// 封装深度测试、混合模式、面剔除等渲染状态

class RHIPipelineState {
public:
    virtual ~RHIPipelineState() = default;

    /// 应用此管线状态
    virtual void Bind() const = 0;

    virtual const RHIPipelineStateDesc& GetDesc() const = 0;
};

} // namespace Engine
