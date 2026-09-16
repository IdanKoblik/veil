#pragma once

#include "../carrier.h"
#include <stddef.h>
#include <veil/handlers/jpeg.h>

#ifdef __cplusplus
extern "C" {
#endif

struct DctCarrier {
    Carrier carrier;

    JCOEF *values;
    size_t slots;

    struct JpegImage jpeg_image;
};

struct DctCarrier *dct_carrier_init(const char *target);

#ifdef __cplusplus
}
#endif
