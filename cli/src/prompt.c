#include "prompt.h"
#include <veil/log.h>

#include <errno.h>
#include <sodium.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

static int read_line_raw(char *out, size_t size) {
    size_t len = 0;
    int overflow = 0;

    for (;;) {
        char c;
        ssize_t got = read(STDIN_FILENO, &c, 1);

        if (got < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (got == 0 || c == '\n')
            break;

        if (c == '\r')
            continue;

        if (len + 1 < size)
            out[len++] = c;
        else
            overflow = 1;
    }

    out[len] = '\0';

    if (overflow) {
        ERROR("Passphrase is longer than %zu characters", size - 1);
        return -1;
    }

    return 0;
}

int read_passphrase(const char *prompt, char *out, size_t size) {
    if (!out || size == 0)
        return -1;

    // Keep it out of swap and out of any core dump for as long as it is live.
    if (sodium_mlock(out, size) != 0)
        DEBUG("Could not lock the passphrase buffer, it may reach swap");

    struct termios original;
    int silent = isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &original) == 0;

    if (silent) {
        struct termios quiet = original;
        quiet.c_lflag &= ~(tcflag_t)ECHO;
        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &quiet) != 0)
            silent = 0;
    }

    printf("%s", prompt);
    fflush(stdout);

    int status = read_line_raw(out, size);

    if (silent) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
        printf("\n");
    }

    if (status < 0)
        sodium_memzero(out, size);

    return status;
}
