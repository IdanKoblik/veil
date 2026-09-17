#include "documents/StreamDocument.hpp"
#include "app/math.hpp"
#include "app/widgets.hpp"

#include <imgui.h>
#include <sstream>
#include <veil/fs/file.h>

void StreamDocument::render(void) {
    const float status_h = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    const float body = ImGui::GetContentRegionAvail().y - status_h;
    const float total = ImGui::GetContentRegionAvail().x;
    const float room = total - 16.0f - 180.0f;

    this->state.left_w = clampf(this->state.left_w, 180.0f, room - this->state.right_w);
    this->state.right_w = clampf(this->state.right_w, 180.0f, room - this->state.left_w);

    ImGui::BeginChild("##source", ImVec2(this->state.left_w, body), ImGuiChildFlags_Borders);
    if (this->show_preview) {
        ui::section_label("PREVIEW");
        ImGui::Spacing();
        this->preview.draw(ImGui::GetContentRegionAvail().x * this->state.zoom);
        ImGui::Spacing();
        this->draw_preview_controls();
        ImGui::Spacing();
        ImGui::Spacing();
    }

    ui::section_label("PROPERTIES");
    ImGui::Spacing();
    ImGui::TextUnformatted(this->name.c_str());
    ImGui::Text("%s, %zu bytes", file_type_name(this->file_type), this->file_size());
    this->draw_properties();
    ImGui::EndChild();

    ImGui::SameLine();
    ui::splitter("##panes", &this->state.left_w, 180.0f, room - this->state.right_w, body);
    ImGui::SameLine();

    ImGui::BeginChild("##streams", ImVec2(0, body), ImGuiChildFlags_Borders);
    this->streams_pane.draw(this->streams.set, this->state);
    ImGui::EndChild();

    this->text.draw(this->streams.set);

    this->draw_status_bar();
}

std::string StreamDocument::summary(void) {
    std::stringstream ss;
    ss << this->path << "  | " << file_type_name(this->file_type);

    if (const struct Stream *stream = this->active_stream())
        ss << "  | " << stream_kind_name(stream->kind) << " stream, " << stream->len << " bytes";

    if (this->streams_pane.has_selection())
        ss << "  | " << this->streams_pane.selection_label();

    return ss.str();
}

void StreamDocument::handle_shortcuts(void) {
    if (this->text.is_open())
        return;

    FileDocument::handle_shortcuts();
}

void StreamDocument::edit_menu(void) {
    const struct Stream *stream = this->active_stream();

    ImGui::Separator();
    if (ImGui::MenuItem("Copy selection", "Ctrl+C", false, stream && this->streams_pane.has_selection()))
        ImGui::SetClipboardText(this->streams_pane.selection_hex(*stream).c_str());
}

void StreamDocument::extra_menus(void) {
    if (ImGui::BeginMenu(this->media_menu())) {
        ImGui::MenuItem("Preview", nullptr, &this->show_preview);

        if (ImGui::MenuItem("Recover Text"))
            this->text.open();

        ImGui::EndMenu();
    }
}

size_t StreamDocument::file_size(void) const {
    const struct Stream *stream = streams_find(&this->streams.set, STREAM_HEX);
    return stream ? stream->len : 0;
}

const struct Stream *StreamDocument::active_stream(void) const {
    if (this->state.stream >= this->streams.set.count)
        return nullptr;

    return &this->streams.set.streams[this->state.stream];
}
