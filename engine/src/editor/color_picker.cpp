#include "engine/editor/color_picker.h"
#include "engine/core/log.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Engine {

void ColorPickerEx::Init() {
    s_RecentColors.clear();
    s_FavoriteColors.clear();
    LOG_INFO("[ColorPicker] 初始化");
}

void ColorPickerEx::Shutdown() {
    LOG_INFO("[ColorPicker] 关闭");
}

bool ColorPickerEx::ColorEdit(const char* label, f32* rgba, bool hdr) {
    bool changed = false;
    ImGui::PushID(label);

    // 预览色块
    ImVec4 color(rgba[0], rgba[1], rgba[2], rgba[3]);
    if (ImGui::ColorButton(label, color, ImGuiColorEditFlags_AlphaPreview, ImVec2(30, 30))) {
        ImGui::OpenPopup("##ColorPickerPopup");
    }
    ImGui::SameLine();
    ImGui::Text("%s", label);

    if (ImGui::BeginPopup("##ColorPickerPopup")) {
        ImGuiColorEditFlags flags = ImGuiColorEditFlags_AlphaBar |
                                     ImGuiColorEditFlags_PickerHueWheel |
                                     ImGuiColorEditFlags_DisplayRGB |
                                     ImGuiColorEditFlags_DisplayHex;
        if (hdr) flags |= ImGuiColorEditFlags_HDR;

        if (ImGui::ColorPicker4("##picker", rgba, flags)) {
            changed = true;
        }

        ImGui::Separator();

        // 十六进制输入
        char hexBuf[16];
        int r = (int)(rgba[0] * 255); r = std::clamp(r, 0, 255);
        int g = (int)(rgba[1] * 255); g = std::clamp(g, 0, 255);
        int b = (int)(rgba[2] * 255); b = std::clamp(b, 0, 255);
        snprintf(hexBuf, sizeof(hexBuf), "#%02X%02X%02X", r, g, b);
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputText("Hex", hexBuf, sizeof(hexBuf))) {
            int hr, hg, hb;
            if (sscanf(hexBuf, "#%02x%02x%02x", &hr, &hg, &hb) == 3) {
                rgba[0] = hr / 255.0f;
                rgba[1] = hg / 255.0f;
                rgba[2] = hb / 255.0f;
                changed = true;
            }
        }

        // HDR 强度
        if (hdr) {
            static float intensity = 1.0f;
            if (ImGui::DragFloat("强度", &intensity, 0.1f, 0.0f, 100.0f)) {
                // 应用到 RGB
            }
        }

        ImGui::Separator();

        // 最近使用
        ImGui::Text("最近:");
        for (size_t i = 0; i < s_RecentColors.size() && i < 16; i++) {
            auto& c = s_RecentColors[i];
            ImVec4 rc(c.R, c.G, c.B, c.A);
            if (ImGui::ColorButton(("##recent" + std::to_string(i)).c_str(), rc, 0, ImVec2(18, 18))) {
                rgba[0] = c.R; rgba[1] = c.G; rgba[2] = c.B; rgba[3] = c.A;
                changed = true;
            }
            if ((i + 1) % 8 != 0) ImGui::SameLine();
        }

        // 收藏
        if (!s_FavoriteColors.empty()) {
            ImGui::Separator();
            ImGui::Text("收藏:");
            for (size_t i = 0; i < s_FavoriteColors.size(); i++) {
                auto& c = s_FavoriteColors[i];
                ImVec4 fc(c.R, c.G, c.B, c.A);
                if (ImGui::ColorButton(("##fav" + std::to_string(i)).c_str(), fc, 0, ImVec2(18, 18))) {
                    rgba[0] = c.R; rgba[1] = c.G; rgba[2] = c.B; rgba[3] = c.A;
                    changed = true;
                }
                if ((i + 1) % 8 != 0) ImGui::SameLine();
            }
        }

        if (ImGui::Button("★ 收藏此颜色")) {
            AddFavorite(rgba[0], rgba[1], rgba[2], rgba[3]);
        }

        ImGui::EndPopup();
    }

    // 记录到最近使用
    if (changed) {
        SavedColor sc{rgba[0], rgba[1], rgba[2], rgba[3]};
        s_RecentColors.insert(s_RecentColors.begin(), sc);
        if (s_RecentColors.size() > MAX_RECENT)
            s_RecentColors.resize(MAX_RECENT);
    }

    ImGui::PopID();
    return changed;
}

bool ColorPickerEx::ColorWheel(const char* label, f32* hsv) {
    ImGui::PushID(label);
    ImGuiColorEditFlags flags = ImGuiColorEditFlags_PickerHueWheel |
                                 ImGuiColorEditFlags_InputHSV |
                                 ImGuiColorEditFlags_DisplayHSV;
    bool changed = ImGui::ColorPicker3(label, hsv, flags);
    ImGui::PopID();
    return changed;
}

bool ColorPickerEx::GradientEditor(const char* label, std::vector<GradientStop>& stops) {
    bool changed = false;
    ImGui::PushID(label);

    ImVec2 barPos = ImGui::GetCursorScreenPos();
    ImVec2 barSize(ImGui::GetContentRegionAvail().x, 30);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // 绘制渐变条
    int segments = (int)barSize.x;
    for (int i = 0; i < segments; i++) {
        f32 t = (f32)i / segments;
        f32 r, g, b, a;
        SampleGradient(stops, t, r, g, b, a);
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
        dl->AddRectFilled(ImVec2(barPos.x + i, barPos.y),
                          ImVec2(barPos.x + i + 1, barPos.y + barSize.y), col);
    }
    dl->AddRect(barPos, ImVec2(barPos.x + barSize.x, barPos.y + barSize.y),
                IM_COL32(100, 100, 100, 255));

    // 停止点标记
    for (size_t i = 0; i < stops.size(); i++) {
        f32 sx = barPos.x + stops[i].Position * barSize.x;
        ImVec2 triPts[3] = {
            {sx - 5, barPos.y + barSize.y},
            {sx + 5, barPos.y + barSize.y},
            {sx, barPos.y + barSize.y + 8}
        };
        dl->AddTriangleFilled(triPts[0], triPts[1], triPts[2],
            ImGui::ColorConvertFloat4ToU32(ImVec4(stops[i].R, stops[i].G, stops[i].B, 1)));
    }

    ImGui::Dummy(ImVec2(barSize.x, barSize.y + 12));

    // 停止点编辑
    for (size_t i = 0; i < stops.size(); i++) {
        ImGui::PushID((int)i);
        ImGui::SetNextItemWidth(60);
        if (ImGui::DragFloat("##pos", &stops[i].Position, 0.01f, 0, 1))
            changed = true;
        ImGui::SameLine();
        f32 col[4] = {stops[i].R, stops[i].G, stops[i].B, stops[i].A};
        if (ImGui::ColorEdit4("##col", col, ImGuiColorEditFlags_NoInputs)) {
            stops[i].R = col[0]; stops[i].G = col[1];
            stops[i].B = col[2]; stops[i].A = col[3];
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X") && stops.size() > 2) {
            stops.erase(stops.begin() + i);
            changed = true;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }

    if (ImGui::SmallButton("+ 添加停止点")) {
        GradientStop ns;
        ns.Position = 0.5f;
        stops.push_back(ns);
        std::sort(stops.begin(), stops.end(), [](const GradientStop& a, const GradientStop& b) {
            return a.Position < b.Position;
        });
        changed = true;
    }

    ImGui::PopID();
    return changed;
}

void ColorPickerEx::SampleGradient(const std::vector<GradientStop>& stops,
                                     f32 t, f32& r, f32& g, f32& b, f32& a) {
    if (stops.empty()) { r = g = b = a = 1; return; }
    if (stops.size() == 1) {
        r = stops[0].R; g = stops[0].G; b = stops[0].B; a = stops[0].A;
        return;
    }
    if (t <= stops.front().Position) {
        r = stops[0].R; g = stops[0].G; b = stops[0].B; a = stops[0].A;
        return;
    }
    if (t >= stops.back().Position) {
        auto& last = stops.back();
        r = last.R; g = last.G; b = last.B; a = last.A;
        return;
    }

    for (size_t i = 0; i < stops.size() - 1; i++) {
        if (t >= stops[i].Position && t <= stops[i+1].Position) {
            f32 lt = (t - stops[i].Position) / (stops[i+1].Position - stops[i].Position);
            r = stops[i].R + (stops[i+1].R - stops[i].R) * lt;
            g = stops[i].G + (stops[i+1].G - stops[i].G) * lt;
            b = stops[i].B + (stops[i+1].B - stops[i].B) * lt;
            a = stops[i].A + (stops[i+1].A - stops[i].A) * lt;
            return;
        }
    }
    r = g = b = a = 1;
}

void ColorPickerEx::AddFavorite(f32 r, f32 g, f32 b, f32 a) {
    s_FavoriteColors.push_back({r, g, b, a});
}

void ColorPickerEx::RenderPalettePanel() {
    if (!ImGui::Begin("调色板##Palette")) { ImGui::End(); return; }

    ImGui::Text("最近使用:");
    for (size_t i = 0; i < s_RecentColors.size(); i++) {
        auto& c = s_RecentColors[i];
        ImVec4 col(c.R, c.G, c.B, c.A);
        ImGui::ColorButton(("##r" + std::to_string(i)).c_str(), col, 0, ImVec2(22, 22));
        if ((i + 1) % 10 != 0) ImGui::SameLine();
    }

    ImGui::Separator();
    ImGui::Text("收藏 (★):");
    for (size_t i = 0; i < s_FavoriteColors.size(); i++) {
        auto& c = s_FavoriteColors[i];
        ImVec4 col(c.R, c.G, c.B, c.A);
        ImGui::ColorButton(("##f" + std::to_string(i)).c_str(), col, 0, ImVec2(22, 22));
        if ((i + 1) % 10 != 0) ImGui::SameLine();
    }

    ImGui::End();
}

} // namespace Engine
