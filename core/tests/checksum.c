#include "veil/fs/checksum.h"
#include "greatest.h"

TEST test_empty_data(void) {
    ASSERT_EQ(0, calculate_checksum(NULL, 0));
    PASS();
}

TEST test_single_byte(void) {
    const unsigned char data[] = {42};

    ASSERT_EQ(42, calculate_checksum(data, 1));
    PASS();
}

TEST test_multiple_bytes(void) {
    const unsigned char data[] = {1, 2, 3, 4, 5};

    ASSERT_EQ(15, calculate_checksum(data, 5));
    PASS();
}

TEST test_zero_bytes(void) {
    const unsigned char data[] = {0, 0, 0, 0};

    ASSERT_EQ(0, calculate_checksum(data, 4));
    PASS();
}

TEST test_overflow(void) {
    const unsigned char data[] = {200, 100};

    /* 300 % 256 = 44 */
    ASSERT_EQ(44, calculate_checksum(data, 2));
    PASS();
}

TEST test_max_values(void) {
    const unsigned char data[] = {255, 255};

    /* 510 % 256 = 254 */
    ASSERT_EQ(254, calculate_checksum(data, 2));
    PASS();
}

TEST test_length_is_respected(void) {
    const unsigned char data[] = {10, 20, 30};

    ASSERT_EQ(30, calculate_checksum(data, 2));
    PASS();
}

SUITE(checksum_suite) {
    RUN_TEST(test_empty_data);
    RUN_TEST(test_single_byte);
    RUN_TEST(test_multiple_bytes);
    RUN_TEST(test_zero_bytes);
    RUN_TEST(test_overflow);
    RUN_TEST(test_max_values);
    RUN_TEST(test_length_is_respected);
}
