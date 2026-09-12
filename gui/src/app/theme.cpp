#include "theme.hpp"
#include <veil/byte.h>

namespace theme {

ImVec4 byte_color(unsigned char byte) {
    switch (classify_byte(byte)) {
    case BYTE_ZERO:
        return byte_zero;
    case BYTE_FILLED:
        return byte_filled;
    case BYTE_WHITESPACE:
        return byte_whitespace;
    case BYTE_PRINTABLE:
        return byte_printable;
    case BYTE_CONTROL:
        return byte_control;
    default:
        return byte_other;
    }
}

void apply(void) {
    ImGuiStyle &style = ImGui::GetStyle();

    style.WindowRounding = 0.0f;
    style.ChildRounding = 3.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;

    ImVec4 *c = style.Colors;
    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = text_dim;
    c[ImGuiCol_WindowBg] = window_bg;
    c[ImGuiCol_ChildBg] = pane_bg;
    c[ImGuiCol_PopupBg] = popup_bg;
    c[ImGuiCol_Border] = border;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.17f, 0.20f, 1.00f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.26f, 0.31f, 1.00f);
    c[ImGuiCol_TitleBg] = bar_bg;
    c[ImGuiCol_TitleBgActive] = bar_bg;
    c[ImGuiCol_MenuBarBg] = bar_bg;
    c[ImGuiCol_ScrollbarBg] = sunken_bg;
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.26f, 0.31f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.31f, 0.34f, 0.40f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive] = accent;
    c[ImGuiCol_CheckMark] = accent_hover;
    c[ImGuiCol_Button] = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.23f, 0.26f, 0.31f, 1.00f);
    c[ImGuiCol_ButtonActive] = accent;
    c[ImGuiCol_Header] = accent_soft;
    c[ImGuiCol_HeaderHovered] = accent_hover;
    c[ImGuiCol_HeaderActive] = accent;
    c[ImGuiCol_Separator] = border;
    c[ImGuiCol_SeparatorHovered] = accent;
    c[ImGuiCol_Tab] = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);
    c[ImGuiCol_TabHovered] = ImVec4(0.20f, 0.23f, 0.28f, 1.00f);
    c[ImGuiCol_TabSelected] = ImVec4(0.17f, 0.19f, 0.23f, 1.00f);
    c[ImGuiCol_TabSelectedOverline] = accent;
    c[ImGuiCol_TabDimmed] = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    c[ImGuiCol_TabDimmedSelected] = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
    c[ImGuiCol_TableHeaderBg] = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
    c[ImGuiCol_TableBorderStrong] = border;
    c[ImGuiCol_TableBorderLight] = ImVec4(0.16f, 0.17f, 0.20f, 1.00f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.02f);
    c[ImGuiCol_TextSelectedBg] = accent_soft;
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.02f, 0.02f, 0.03f, 0.60f);
}

} // theme
