#pragma once

#include <stddef.h>

typedef struct Carrier {
    int (*write)(struct Carrier* carrier, size_t slot, unsigned char bit);
    unsigned char (*read)(struct Carrier *carrier, size_t slot);
    int (*capacity)(struct Carrier *carrier);
    int (*free)(struct Carrier* carrier);
} Carrier;

Carrier *figure_carrier(const char* target);
