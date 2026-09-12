#pragma once

#include <cstddef>

struct ViewState {
    static constexpr float zoom_min = 0.25f;
    static constexpr float zoom_max = 4.00f;
    static constexpr float zoom_step = 1.25f;

    float right_w = 320.0f;
    float left_w = 300.0f;
    float zoom = 1.0f;
    size_t stream = 0;
    bool find_open = false;
};
