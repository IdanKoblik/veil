#pragma once

inline float clampf(float v, float lo, float hi) {
    if (hi < lo)
        return lo;

    return v < lo ? lo : (v > hi ? hi : v);
}