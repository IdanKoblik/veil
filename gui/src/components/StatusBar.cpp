#include "components/StatusBar.hpp"
#include "app/theme.hpp"
#include <imgui.h>

void StatusBar::draw(const std::string &left) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::bar_bg);
    ImGui::BeginChild("##status", ImVec2(0, ImGui::GetTextLineHeightWithSpacing()));
    ImGui::PushStyleColor(ImGuiCol_Text, theme::text_dim);

    /* A path is not a format string, whatever it happens to hold. */
    ImGui::TextUnformatted(left.c_str());

    // TODO right side

    ImGui::PopStyleColor();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}
