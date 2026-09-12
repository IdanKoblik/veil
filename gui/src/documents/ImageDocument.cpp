#include "documents/ImageDocument.hpp"
#include "app/math.hpp"
#include "app/widgets.hpp"

#include <imgui.h>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <veil/fs/file.h>

void ImageDocument::render(void) {
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
        ImGui::Spacing();
    }

    ui::section_label("PROPERTIES");
    ImGui::Spacing();
    ImGui::TextUnformatted(this->name.c_str());
    ImGui::Text("%s, %zu bytes", file_type_name(this->file_type), this->file_size());
    ImGui::Text("%d x %d, %d channels", this->pixels.buffer.width, this->pixels.buffer.height, this->pixels.buffer.channels);
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

void ImageDocument::open(const std::string &target) {
    const std::string target_name = file_name_of(target);

    this->file_type = get_file_type(target.c_str());
    if (this->file_type == TYPE_NOT_FOUND)
        throw std::runtime_error("No such file: " + target);

    if (!is_image_file(this->file_type))
        throw std::runtime_error("Not a valid supported image type: " + target_name);

    Pixels decoded;
    if (pixels_load(target.c_str(), &decoded.buffer) != 0)
        throw std::runtime_error("Could not decode " + target_name);

    Streams built;
    if (streams_construct(target.c_str(), &built.set) != 0)
        throw std::runtime_error("Could not read " + target_name);

    std::swap(this->pixels.buffer, decoded.buffer);
    std::swap(this->streams.set, built.set);
    this->path = target;
    this->name = target_name;
    this->state.stream = 0;

    this->preview.load(this->pixels.buffer);
}

std::string ImageDocument::summary(void) {
    std::stringstream ss;
    ss << this->path << "  | " << file_type_name(this->file_type);

    if (const struct Stream *stream = this->active_stream())
        ss << "  | " << stream_kind_name(stream->kind) << " stream, " << stream->len << " bytes";

    if (this->streams_pane.has_selection())
        ss << "  | " << this->streams_pane.selection_label();

    return ss.str();
}

void ImageDocument::handle_shortcuts(void) {
    /* Ctrl+F belongs to the popout while it is up. */
    if (this->text.is_open())
        return;

    FileDocument::handle_shortcuts();
}

void ImageDocument::edit_menu(void) {
    const struct Stream *stream = this->active_stream();

    ImGui::Separator();
    if (ImGui::MenuItem("Copy selection", "Ctrl+C", false, stream && this->streams_pane.has_selection()))
        ImGui::SetClipboardText(this->streams_pane.selection_hex(*stream).c_str());
}

void ImageDocument::extra_menus(void) {
    if (ImGui::BeginMenu("Image")) {
        ImGui::MenuItem("Preview", nullptr, &this->show_preview);

        if (ImGui::MenuItem("Recover Text"))
            this->text.open();

        ImGui::EndMenu();
    }
}

size_t ImageDocument::file_size(void) const {
    const struct Stream *stream = streams_find(&this->streams.set, STREAM_HEX);
    return stream ? stream->len : 0;
}

const struct Stream *ImageDocument::active_stream(void) const {
    if (this->state.stream >= this->streams.set.count)
        return nullptr;

    return &this->streams.set.streams[this->state.stream];
}
