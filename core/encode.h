#pragma once

#include "crypto/passphrase.h"

int encode(const char *target, const char *data_file, const char *output, char passphrase[PASSPHRASE_MAX]);