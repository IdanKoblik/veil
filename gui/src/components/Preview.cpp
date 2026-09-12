#include "components/Preview.hpp"
#include "app/theme.hpp"

Preview::~Preview() {
    this->unload();
}

void Preview::load(const struct PixelBuffer &pixels) {
    this->unload();

    const int format = format_of(pixels.channels);
    if (!pixels.samples || pixels.width <= 0 || pixels.height <= 0 || format == 0)
        return;

    const Image source = {pixels.samples, pixels.width, pixels.height, 1, format};

    const int longest = std::max(source.width, source.height);
    if (longest <= max_edge) {
        this->texture = LoadTextureFromImage(source);
    } else {
        Image scaled = ImageCopy(source);
        ImageResize(&scaled, std::max(1, source.width * max_edge / longest), std::max(1, source.height * max_edge / longest));
        this->texture = LoadTextureFromImage(scaled);
        UnloadImage(scaled);
    }

    SetTextureFilter(this->texture, TEXTURE_FILTER_BILINEAR);
}

void Preview::unload(void) {
    if (this->texture.id != 0)
        UnloadTexture(this->texture);

    this->texture = Texture2D{};
}

void Preview::draw(float edge) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    if (this->texture.id != 0) {
        const float scale = std::min(edge / this->texture.width, edge / this->texture.height);
        const int width = static_cast<int>(this->texture.width * scale);
        const int height = static_cast<int>(this->texture.height * scale);

        ImGui::SetCursorScreenPos(ImVec2(origin.x + (edge - width) * 0.5f, origin.y + (edge - height) * 0.5f));
        rlImGuiImageSize(&this->texture, width, height);
    }

    ImGui::GetWindowDrawList()->AddRect(origin, ImVec2(origin.x + edge, origin.y + edge), ImGui::GetColorU32(theme::border));
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + edge));
}
