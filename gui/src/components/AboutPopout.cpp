#include "components/AboutPopout.hpp"
#include <imgui.h>

static const char *const popup_title = "About veil";

void AboutPopout::open(void) {
    this->pending = true;
}

void AboutPopout::draw(void) {
    if (this->pending) {
        ImGui::OpenPopup(popup_title);
        this->pending = false;
    }

    if (!ImGui::BeginPopupModal(popup_title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::TextUnformatted("veil");
    ImGui::TextDisabled("A steganography toolkit.");
    ImGui::Separator();
    ImGui::TextWrapped("https://github.com/IdanKoblik/veil");
    ImGui::Separator();

    if (ImGui::Button("Close", ImVec2(120, 0)))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}
