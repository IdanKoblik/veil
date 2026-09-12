#pragma once

#include <veil/analysis/stream.h>

struct Streams {
    struct StreamSet set {};

    Streams() = default;
    Streams(const Streams &) = delete;
    Streams &operator=(const Streams &) = delete;

    ~Streams() {
        streams_free(&this->set);
    }
};
