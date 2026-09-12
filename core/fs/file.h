#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum FileType { TYPE_PNG_IMAGE, TYPE_JPEG_IMAGE, TYPE_UNKNOWN, TYPE_NOT_FOUND };

int is_image_file(enum FileType type);

const char *file_type_name(enum FileType type);

enum FileType get_file_type(const char *target);
int read_file_raw_data(const char *target, unsigned char **data, size_t *data_len);
int write_to_file_raw_data(const char *target, const unsigned char *data, size_t data_len);

#ifdef __cplusplus
}
#endif
