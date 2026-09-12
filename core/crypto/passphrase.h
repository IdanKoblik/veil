#pragma once

#include <sodium.h>

#define PASSPHRASE_MAX 256

static inline void passphrase_wipe(char (*passphrase)[PASSPHRASE_MAX]) {
    sodium_munlock(*passphrase, sizeof(*passphrase));
}

#define PASSPHRASE(name) __attribute__((cleanup(passphrase_wipe))) char name[PASSPHRASE_MAX]
