#pragma once

#include "../carrier.h"

#ifdef __cplusplus
extern "C" {
#endif

struct LsbCarrier {
    Carrier carrier;

    unsigned char *pixels;
    size_t slots;

    size_t colors;
    size_t channels;
    size_t height;
    size_t width;
};

struct LsbCarrier *lsb_carrier_init(const char *target);

#ifdef __cplusplus
}
#endif
