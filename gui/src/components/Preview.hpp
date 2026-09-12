#pragma once

#include "app/math.hpp"
#include "model/Pixels.hpp"
#include <algorithm>
#include <cstdlib>
#include <imgui.h>
#include <rlImGui.h>
#include <veil/analysis/image/inspect.h>

class Preview {
public:
    static constexpr int max_edge = 1024;

    Preview() = default;
    Preview(const Preview &) = delete;
    Preview &operator=(const Preview &) = delete;

    ~Preview();

    void load(const struct PixelBuffer &pixels); 
    void unload(void);
    void draw(float edge);

private:
    Texture2D texture {};

    static int format_of(int channels) {
        switch (channels) {
        case 1: return PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
        case 2: return PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA;
        case 3: return PIXELFORMAT_UNCOMPRESSED_R8G8B8;
        case 4: return PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        default: return 0;
        }
    }
};
