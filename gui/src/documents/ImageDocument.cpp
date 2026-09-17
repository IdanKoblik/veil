#include "documents/ImageDocument.hpp"

#include <imgui.h>
#include <stdexcept>
#include <utility>
#include <veil/fs/file.h>

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

void ImageDocument::draw_properties(void) {
    ImGui::Text("%d x %d, %d channels", this->pixels.buffer.width, this->pixels.buffer.height, this->pixels.buffer.channels);
}
