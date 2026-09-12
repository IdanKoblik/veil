#include "greatest.h"

#include "veil/byte.h"

TEST classify_calls_out_the_two_extremes(void) {
    ASSERT_EQ(BYTE_ZERO, classify_byte(0x00));
    ASSERT_EQ(BYTE_FILLED, classify_byte(0xFF));
    PASS();
}

TEST classify_knows_whitespace_from_text(void) {
    ASSERT_EQ(BYTE_WHITESPACE, classify_byte(' '));
    ASSERT_EQ(BYTE_WHITESPACE, classify_byte('\t'));
    ASSERT_EQ(BYTE_WHITESPACE, classify_byte('\n'));
    ASSERT_EQ(BYTE_WHITESPACE, classify_byte('\r'));

    ASSERT_EQ(BYTE_PRINTABLE, classify_byte('!'));
    ASSERT_EQ(BYTE_PRINTABLE, classify_byte('A'));
    ASSERT_EQ(BYTE_PRINTABLE, classify_byte('~'));
    PASS();
}

TEST classify_takes_the_control_range(void) {
    ASSERT_EQ(BYTE_CONTROL, classify_byte(0x01));
    ASSERT_EQ(BYTE_CONTROL, classify_byte(0x1F));
    ASSERT_EQ(BYTE_CONTROL, classify_byte(0x7F));
    PASS();
}

TEST classify_leaves_the_high_half_other(void) {
    ASSERT_EQ(BYTE_OTHER, classify_byte(0x80));
    ASSERT_EQ(BYTE_OTHER, classify_byte(0xC3));
    ASSERT_EQ(BYTE_OTHER, classify_byte(0xFE));
    PASS();
}

TEST classify_covers_every_byte(void) {
    for (int byte = 0; byte <= 0xFF; byte++) {
        const enum ByteClass of = classify_byte((unsigned char)byte);

        ASSERT(of >= BYTE_ZERO && of <= BYTE_OTHER);

        if (of == BYTE_PRINTABLE)
            ASSERT(byte >= 0x20 && byte <= 0x7E);
    }

    PASS();
}

SUITE(byte_suite) {
    RUN_TEST(classify_calls_out_the_two_extremes);
    RUN_TEST(classify_knows_whitespace_from_text);
    RUN_TEST(classify_takes_the_control_range);
    RUN_TEST(classify_leaves_the_high_half_other);
    RUN_TEST(classify_covers_every_byte);
}
