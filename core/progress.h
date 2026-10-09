#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ProgressUpdate)(const char *stage, size_t done, size_t total, void *ctx);
typedef void (*ProgressFinish)(void *ctx);

void progress_set(ProgressUpdate update, ProgressFinish finish, void *ctx);

void progress_update(const char *stage, size_t done, size_t total);
void progress_finish(void);

#ifdef __cplusplus
}
#endif
