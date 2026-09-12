#pragma once

#include <veil/crypto/passphrase.h>
#include <sodium.h>
#include <stddef.h>

int read_passphrase(const char *prompt, char *out, size_t size);
