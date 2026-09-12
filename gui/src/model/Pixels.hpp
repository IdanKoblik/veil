#pragma once

#include <veil/analysis/image/inspect.h>

struct Pixels {
    struct PixelBuffer buffer {};

    Pixels() = default;
    Pixels(const Pixels &) = delete;
    Pixels &operator=(const Pixels &) = delete;

    ~Pixels() {
        pixels_free(&this->buffer);
    }
};
