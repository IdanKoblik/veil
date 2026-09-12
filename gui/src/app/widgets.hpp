#pragma once

#include "app/math.hpp"
#include "app/theme.hpp"
#include <imgui.h>

namespace ui {

inline void section_label(const char *text) {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_dim);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
};

inline void centered_text(const char *text) {
    const float x = (ImGui::GetWindowWidth() - ImGui::CalcTextSize(text).x) * 0.5f;
    if (x > ImGui::GetCursorPosX())
        ImGui::SetCursorPosX(x);

    ImGui::TextUnformatted(text);
};

inline void centered_label(const char *text) {
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_dim);
    centered_text(text);
    ImGui::PopStyleColor();
};

inline bool splitter(const char *id, float *width, float lo, float hi, float height) {
    ImGui::PushID(id);
    ImGui::InvisibleButton("##splitter", ImVec2(6.0f, height));

    const bool held = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();

    if (held)
        *width = clampf(*width + ImGui::GetIO().MouseDelta.x, lo, hi);

    if (held || hovered)
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const ImVec4 line = held ? theme::accent : (hovered ? theme::accent_soft : theme::border);

    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2((min.x + max.x) * 0.5f - 1.0f, min.y), ImVec2((min.x + max.x) * 0.5f + 1.0f, max.y), ImGui::GetColorU32(line));
    ImGui::PopID();

    return held;
};

} // ui
