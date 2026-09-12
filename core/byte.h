#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum ByteClass {
    BYTE_ZERO,
    BYTE_FILLED,
    BYTE_WHITESPACE,
    BYTE_PRINTABLE,
    BYTE_CONTROL,
    BYTE_OTHER,
};

enum ByteClass classify_byte(unsigned char byte);

#ifdef __cplusplus
}
#endif
