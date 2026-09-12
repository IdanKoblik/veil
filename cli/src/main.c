#include "cmd/command.h"
#include "src/usage.h"
#include <veil/log.h>
#include <jpeglib.h>
#include <sodium.h>
#include <stddef.h>
#include <stdio.h>

#include "flag.h"

#include <sys/resource.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif

static void harden_process(void) {
    struct rlimit no_core = {.rlim_cur = 0, .rlim_max = 0};
    if (setrlimit(RLIMIT_CORE, &no_core) != 0)
        DEBUG("Could not disable core dumps");

#if defined(__linux__) && defined(PR_SET_DUMPABLE)
    if (prctl(PR_SET_DUMPABLE, 0, 0, 0, 0) != 0)
        DEBUG("Could not clear the dumpable flag");
#endif
}

int main(int argc, char *argv[]) {
    if (sodium_init() < 0) {
        ERROR("Failed to initialise libsodium");
        return 1;
    }

    if (argc > 1 && strcmp(argv[1], "--verbose") == 0) {
        verbose = 1;
        argv[1] = argv[0];
        argc--;
        argv++;
    }

    harden_process();

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    DEBUG("Loaded %zu commands", COMMAND_COUNT);
    const char *cmd_name = argv[1];
    const struct Command *cmd = find_command(cmd_name);
    argc -= 2;
    argv += 2;

    if (!cmd) {
        ERROR("Subcommand %s was not found", cmd_name);
        return 1;
    }

    int exec_status = cmd->exec(argc, argv);
    if (exec_status == EXEC_USAGE_ERROR) {
        printf("%s", cmd->usage);
        return 1;
    }

    if (exec_status == EXEC_GENERIC_ERROR) {
        ERROR("An error accrued while running %s subcommand", cmd->name);
        return 1;
    }

    return 0;
}
