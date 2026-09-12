#include "greatest.h"
#include <sodium.h>

SUITE_EXTERN(file_suite);
SUITE_EXTERN(checksum_suite);
SUITE_EXTERN(stream_suite);
SUITE_EXTERN(byte_suite);
SUITE_EXTERN(codec_suite);
SUITE_EXTERN(prng_suite);
SUITE_EXTERN(payload_suite);
SUITE_EXTERN(container_suite);
SUITE_EXTERN(image_suite);
SUITE_EXTERN(lsb_suite);
SUITE_EXTERN(dct_suite);
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    if (sodium_init() < 0) {
        fprintf(stderr, "Failed to initialise libsodium\n");
        return 1;
    }

    GREATEST_MAIN_BEGIN();
    RUN_SUITE(file_suite);
    RUN_SUITE(checksum_suite);
    RUN_SUITE(stream_suite);
    RUN_SUITE(byte_suite);
    RUN_SUITE(codec_suite);
    RUN_SUITE(prng_suite);
    RUN_SUITE(payload_suite);
    RUN_SUITE(container_suite);
    RUN_SUITE(image_suite);
    RUN_SUITE(lsb_suite);
    RUN_SUITE(dct_suite);
    GREATEST_MAIN_END();
}
