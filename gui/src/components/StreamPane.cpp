#include "components/StreamPane.hpp"
#include "app/widgets.hpp"
#include <imgui.h>

void StreamPane::draw(const struct StreamSet &set, ViewState &state) {
    ui::section_label("STREAMS");
    ImGui::Spacing();

    if (set.count == 0) {
        ImGui::TextDisabled("No stream could be read");
        return;
    }

    if (!ImGui::BeginTabBar("##stream_kinds"))
        return;

    for (size_t i = 0; i < set.count; i++) {
        const struct Stream &stream = set.streams[i];

        if (!ImGui::BeginTabItem(stream_kind_name(stream.kind)))
            continue;

        state.stream = i;

        if (state.find_open) {
            if (this->find.draw(stream, &state.find_open))
                this->hex.select(this->find.offset(), this->find.length());
        } else {
            this->find.close();
        }

        this->hex.draw(stream, this->find.matches());
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}
