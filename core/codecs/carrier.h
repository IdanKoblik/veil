#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Carrier {
    int (*write)(struct Carrier* carrier, size_t slot, unsigned char bit);
    unsigned char (*read)(struct Carrier *carrier, size_t slot);
    size_t (*capacity)(struct Carrier *carrier);
    int (*save)(struct Carrier *carrier, const char *output);
    int (*free)(struct Carrier* carrier);
} Carrier;

Carrier *figure_carrier(const char* target);

#ifdef __cplusplus
}
#endif
