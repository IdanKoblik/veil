#pragma once

#include <imgui.h>

namespace theme {

constexpr ImVec4 bar_bg = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
constexpr ImVec4 window_bg = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
constexpr ImVec4 pane_bg = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
constexpr ImVec4 popup_bg = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
constexpr ImVec4 sunken_bg = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
constexpr ImVec4 border = ImVec4(0.20f, 0.21f, 0.25f, 1.00f);
constexpr ImVec4 accent = ImVec4(0.20f, 0.47f, 0.76f, 1.00f);
constexpr ImVec4 accent_hover = ImVec4(0.26f, 0.56f, 0.86f, 1.00f);
constexpr ImVec4 accent_soft = ImVec4(0.20f, 0.47f, 0.76f, 0.35f);
constexpr ImVec4 text = ImVec4(0.85f, 0.86f, 0.88f, 1.00f);
constexpr ImVec4 text_dim = ImVec4(0.48f, 0.51f, 0.57f, 1.00f);
constexpr ImVec4 text_faint = ImVec4(0.34f, 0.36f, 0.41f, 1.00f);
constexpr ImVec4 match = ImVec4(0.85f, 0.65f, 0.25f, 0.30f);
constexpr ImVec4 match_current = ImVec4(0.96f, 0.74f, 0.30f, 0.65f);

constexpr ImVec4 byte_zero = ImVec4(0.36f, 0.38f, 0.43f, 1.00f);
constexpr ImVec4 byte_filled = ImVec4(0.85f, 0.47f, 0.36f, 1.00f);
constexpr ImVec4 byte_whitespace = ImVec4(0.38f, 0.58f, 0.56f, 1.00f);
constexpr ImVec4 byte_printable = ImVec4(0.55f, 0.76f, 0.96f, 1.00f);
constexpr ImVec4 byte_control = ImVec4(0.72f, 0.53f, 0.82f, 1.00f);
constexpr ImVec4 byte_other = ImVec4(0.74f, 0.76f, 0.80f, 1.00f);

ImVec4 byte_color(unsigned char byte);
void apply(void);

} // theme
