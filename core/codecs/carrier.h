#pragma once

#include <stddef.h>

typedef struct Carrier {
    int (*write)(struct Carrier* carrier, size_t slot, unsigned char bit);
    int (*capacity)(struct Carrier *carrier);
    int (*free)(struct Carrier* carrier);
} Carrier;

inline Carrier *figure_carrier(const char* target);
