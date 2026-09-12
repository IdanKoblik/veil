#include "components/TextPopout.hpp"
#include "app/theme.hpp"
#include "app/widgets.hpp"

#include <cstdio>
#include <imgui.h>

static const char *const popout_title = "Recovered text";

void TextPopout::open(void) {
    this->pending = true;
}

void TextPopout::draw(const struct StreamSet &set) {
    if (this->pending) {
        ImGui::OpenPopup(popout_title);
        this->pending = false;
    }

    ImGui::SetNextWindowSize(ImVec2(760.0f, 520.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(popout_title, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        this->shown = false;
        this->find_open = false;
        this->find.close();
        return;
    }

    this->shown = true;

    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_F))
        this->find_open = !this->find_open;

    if (ImGui::Button("Close"))
        ImGui::CloseCurrentPopup();

    ImGui::SameLine();
    ui::section_label("PRINTABLE BYTES OF EACH STREAM, THE REST AS DOTS");
    ImGui::Spacing();

    if (!ImGui::BeginTabBar("##text_streams")) {
        ImGui::EndPopup();
        return;
    }

    for (size_t i = 0; i < set.count; i++) {
        const struct Stream &stream = set.streams[i];

        if (!ImGui::BeginTabItem(stream_kind_name(stream.kind)))
            continue;

        this->stream = i;

        if (this->find_open) {
            if (this->find.draw(stream, &this->find_open))
                this->view.reveal(this->find.offset());

            ImGui::Spacing();
        } else {
            this->find.close();
        }

        this->view.draw(stream, this->find.matches());

        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
    ImGui::EndPopup();
}
