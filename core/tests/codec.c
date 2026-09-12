#include "greatest.h"

#include "veil/analysis/image/inspect.h"
#include "veil/codecs/codec.h"

TEST codec_names_map_to_codecs(void) {
    ASSERT_EQ(CODEC_LSB_MATCHING, str_to_codec("lsbm"));
    ASSERT_EQ(CODEC_LSB_REPLACEMENT, str_to_codec("lsbr"));
    ASSERT_EQ(CODEC_DCT, str_to_codec("dct"));
    PASS();
}

TEST codec_names_are_exact(void) {
    ASSERT_EQ(CODEC_UNKNOWN, str_to_codec(NULL));
    ASSERT_EQ(CODEC_UNKNOWN, str_to_codec(""));
    ASSERT_EQ(CODEC_UNKNOWN, str_to_codec("LSBM"));
    ASSERT_EQ(CODEC_UNKNOWN, str_to_codec("lsb"));
    ASSERT_EQ(CODEC_UNKNOWN, str_to_codec("lsbmm"));
    PASS();
}

TEST colour_channels_leave_out_alpha(void) {
    ASSERT_EQ((size_t)1, pixel_color_channels(1));
    ASSERT_EQ((size_t)1, pixel_color_channels(2));
    ASSERT_EQ((size_t)3, pixel_color_channels(3));
    ASSERT_EQ((size_t)3, pixel_color_channels(4));
    PASS();
}

TEST slots_run_straight_through_when_nothing_is_skipped(void) {
    for (size_t slot = 0; slot < 12; slot++)
        ASSERT_EQ(slot, pixel_slot_to_sample(slot, 3, 3));

    PASS();
}

TEST slots_step_over_the_alpha_sample(void) {
    ASSERT_EQ((size_t)0, pixel_slot_to_sample(0, 3, 4));
    ASSERT_EQ((size_t)1, pixel_slot_to_sample(1, 3, 4));
    ASSERT_EQ((size_t)2, pixel_slot_to_sample(2, 3, 4));
    ASSERT_EQ((size_t)4, pixel_slot_to_sample(3, 3, 4));
    ASSERT_EQ((size_t)5, pixel_slot_to_sample(4, 3, 4));
    ASSERT_EQ((size_t)8, pixel_slot_to_sample(6, 3, 4));

    ASSERT_EQ((size_t)0, pixel_slot_to_sample(0, 1, 2));
    ASSERT_EQ((size_t)2, pixel_slot_to_sample(1, 1, 2));
    ASSERT_EQ((size_t)4, pixel_slot_to_sample(2, 1, 2));
    PASS();
}

SUITE(codec_suite) {
    RUN_TEST(codec_names_map_to_codecs);
    RUN_TEST(codec_names_are_exact);
    RUN_TEST(colour_channels_leave_out_alpha);
    RUN_TEST(slots_run_straight_through_when_nothing_is_skipped);
    RUN_TEST(slots_step_over_the_alpha_sample);
}
