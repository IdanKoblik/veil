#pragma once

#include <veil/codecs/video/h264.h>

struct Video {
    struct H264Carrier *carrier = nullptr;

    Video() = default;
    Video(const Video &) = delete;
    Video &operator=(const Video &) = delete;

    ~Video() {
        if (this->carrier)
            this->carrier->carrier.free(&this->carrier->carrier);
    }
};
