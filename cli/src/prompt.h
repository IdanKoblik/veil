#pragma once

#include <sodium.h>
#include <stddef.h>
#include <veil/crypto/passphrase.h>

int read_passphrase(const char *prompt, char *out, size_t size);
