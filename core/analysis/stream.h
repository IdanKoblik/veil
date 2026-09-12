#pragma once

#include <stddef.h>

#include "../fs/file.h"

#ifdef __cplusplus
extern "C" {
#endif

enum StreamKind { STREAM_HEX, STREAM_LSB, STREAM_DCT };

#define STREAM_KIND_COUNT 3

struct Stream {
    enum StreamKind kind;

    unsigned char *bytes;
    size_t len;

    size_t slots;
};

struct StreamSet {
    struct Stream streams[STREAM_KIND_COUNT];
    size_t count;

    enum FileType file_type;
};

const char *stream_kind_name(enum StreamKind kind);

int stream_kind_available(enum StreamKind kind, enum FileType type);

int stream_load(const char *target, enum StreamKind kind, struct Stream *out);
void stream_free(struct Stream *stream);

int streams_construct(const char *target, struct StreamSet *set);
void streams_free(struct StreamSet *set);

const struct Stream *streams_find(const struct StreamSet *set, enum StreamKind kind);

#ifdef __cplusplus
}
#endif
