#include "progress_bar.h"

#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <veil/progress.h>

#define BAR_MAX_WIDTH 40
#define REDRAW_INTERVAL 0.1

struct ProgressBar {
    const char *stage;
    double started;
    double drawn;
    int open;
};

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static int terminal_width(void) {
    struct winsize size;
    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0)
        return size.ws_col;

    return 80;
}

static void duration_format(char *out, size_t size, double seconds) {
    const long total = (long)(seconds + 0.5);
    if (total >= 3600)
        snprintf(out, size, "%ld:%02ld:%02ld", total / 3600, total / 60 % 60, total % 60);
    else
        snprintf(out, size, "%ld:%02ld", total / 60, total % 60);
}

static void draw(struct ProgressBar *bar, size_t done, size_t total, double elapsed) {
    const double fraction = total ? (double)(done < total ? done : total) / (double)total : 0;
    const double rate = elapsed > 0 ? (double)done / elapsed : 0;

    char eta[32] = "--:--";
    if (done >= total)
        duration_format(eta, sizeof(eta), elapsed);
    else if (rate > 0 && elapsed > 0.5)
        duration_format(eta, sizeof(eta), (double)(total - done) / rate);

    // Padded to the width the numbers will reach, so the bar doesn't shift as they grow.
    char total_text[24];
    const int digits = snprintf(total_text, sizeof(total_text), "%zu", total);

    char stats[96];
    snprintf(stats, sizeof(stats), " %3d%%  %*zu/%s  %5.0f/s  %s %7s", (int)(fraction * 100), digits, done, total_text, rate, done >= total ? "took" : " eta", eta);

    char label[64];
    snprintf(label, sizeof(label), "[~] %s ", bar->stage);

    // Squeezed to the terminal, so a narrow window doesn't wrap it into a new line on every redraw.
    int width = terminal_width() - (int)strlen(label) - (int)strlen(stats) - 3;
    if (width > BAR_MAX_WIDTH)
        width = BAR_MAX_WIDTH;

    fprintf(stderr, "\r\033[K%s", label);
    if (width >= 10) {
        const int filled = (int)(fraction * width);
        fputc('[', stderr);
        for (int i = 0; i < width; i++)
            fputs(i < filled ? "#" : (i == filled && done < total ? ">" : "."), stderr);
        fputc(']', stderr);
    }

    fputs(stats, stderr);
    fflush(stderr);
}

static void update(const char *stage, size_t done, size_t total, void *ctx) {
    struct ProgressBar *bar = ctx;
    const double t = now();

    if (!bar->open || bar->stage != stage) {
        if (bar->open)
            fputc('\n', stderr);

        bar->stage = stage;
        bar->started = t;
        bar->drawn = 0;
        bar->open = 1;
    }

    // Stages report every frame, redrawing that often costs more than the frames themselves.
    if (done < total && t - bar->drawn < REDRAW_INTERVAL)
        return;

    bar->drawn = t;
    draw(bar, done, total, t - bar->started);
}

static void finish(void *ctx) {
    struct ProgressBar *bar = ctx;
    if (!bar->open)
        return;

    fputc('\n', stderr);
    fflush(stderr);
    bar->open = 0;
}

void progress_bar_install(void) {
    if (!isatty(STDERR_FILENO))
        return;

    static struct ProgressBar bar;
    progress_set(update, finish, &bar);
}
