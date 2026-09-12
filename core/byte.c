#include "byte.h"

enum ByteClass classify_byte(unsigned char byte) {
    if (byte == 0x00)
        return BYTE_ZERO;

    if (byte == 0xFF)
        return BYTE_FILLED;

    if (byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r')
        return BYTE_WHITESPACE;

    if (byte >= 0x20 && byte <= 0x7E)
        return BYTE_PRINTABLE;

    if (byte < 0x20 || byte == 0x7F)
        return BYTE_CONTROL;

    return BYTE_OTHER;
}
