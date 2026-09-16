#pragma once

#include "crypto/passphrase.h"

int encode(const char *target, const char *output, char passphrase[PASSPHRASE_MAX], int is_pipe);