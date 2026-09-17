#include "progress.h"

static ProgressUpdate progress_update_fn = NULL;
static ProgressFinish progress_finish_fn = NULL;
static void *progress_ctx = NULL;

void progress_set(ProgressUpdate update, ProgressFinish finish, void *ctx) {
    progress_update_fn = update;
    progress_finish_fn = finish;
    progress_ctx = ctx;
}

void progress_update(const char *stage, size_t done, size_t total) {
    if (progress_update_fn)
        progress_update_fn(stage, done, total, progress_ctx);
}

void progress_finish(void) {
    if (progress_finish_fn)
        progress_finish_fn(progress_ctx);
}
