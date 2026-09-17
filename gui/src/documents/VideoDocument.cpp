#include "documents/VideoDocument.hpp"

#include <cfloat>
#include <imgui.h>
#include <stdexcept>
#include <utility>
#include <veil/analysis/video/inspect.h>
#include <veil/fs/file.h>
#include <veil/log.h>

void VideoDocument::open(const std::string &target) {
    const std::string target_name = file_name_of(target);

    this->file_type = get_file_type(target.c_str());
    if (this->file_type == TYPE_NOT_FOUND)
        throw std::runtime_error("No such file: " + target);

    if (!is_video_file(this->file_type))
        throw std::runtime_error("Not a valid supported video type: " + target_name);

    Video decoded;
    decoded.carrier = h264_carrier_init(target.c_str());
    if (!decoded.carrier)
        throw std::runtime_error("Could not decode " + target_name);

    Streams built;
    built.set.file_type = this->file_type;

    if (stream_load(target.c_str(), STREAM_HEX, &built.set.streams[built.set.count]) != 0)
        throw std::runtime_error("Could not read " + target_name);
    built.set.count++;

    if (stream_take_h264(decoded.carrier, &built.set.streams[built.set.count]) == 0)
        built.set.count++;
    else
        WARN("Leaving out the LSB stream of %s", target.c_str());

    std::swap(this->video.carrier, decoded.carrier);
    std::swap(this->streams.set, built.set);
    this->path = target;
    this->name = target_name;
    this->state.stream = 0;

    this->frame = 0;
    this->show_frame(0);
}

void VideoDocument::draw_properties(void) {
    const struct H264Carrier *carrier = this->video.carrier;
    if (!carrier)
        return;

    ImGui::Text("%d x %d, %zu frames", carrier->width, carrier->height, carrier->frame_count);

    if (carrier->frame_rate.num > 0)
        ImGui::Text("%.2f fps", av_q2d(carrier->frame_rate));

    ImGui::Text("%zu LSB slots", carrier->slots);
}

void VideoDocument::draw_preview_controls(void) {
    const struct H264Carrier *carrier = this->video.carrier;
    if (!carrier || carrier->frame_count < 2)
        return;

    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderInt("##frame", &this->frame, 0, static_cast<int>(carrier->frame_count) - 1, "Frame %d");

    if (ImGui::IsItemDeactivatedAfterEdit())
        this->show_frame(this->frame);
}

void VideoDocument::show_frame(int index) {
    struct PixelBuffer pixels{};
    if (video_frame_load(this->video.carrier, static_cast<size_t>(index), &pixels) != 0)
        return;

    this->preview.load(pixels);

    video_frame_free(&pixels);
}
