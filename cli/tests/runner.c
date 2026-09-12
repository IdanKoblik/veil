#include "greatest.h"

SUITE_EXTERN(e2e_suite);
GREATEST_MAIN_DEFS();

int main(int argc, char **argv) {
    GREATEST_MAIN_BEGIN();
    RUN_SUITE(e2e_suite);
    GREATEST_MAIN_END();
}
