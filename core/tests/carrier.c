#include "greatest.h"

#include "helpers.h"
#include "veil/codecs/carrier.h"

static enum greatest_test_res bits_round_trip(Carrier *carrier) {
    const size_t slots = carrier->capacity(carrier);
    ASSERT(slots > 0);

    for (size_t slot = 0; slot < slots; slot++)
        ASSERT_EQ(0, carrier->write(carrier, slot, (unsigned char)((slot * 7 + 3) % 5 < 2)));

    for (size_t slot = 0; slot < slots; slot++)
        ASSERT_EQ((unsigned char)((slot * 7 + 3) % 5 < 2), carrier->read(carrier, slot));

    PASS();
}

static enum greatest_test_res bits_survive_a_save(char *image) {
    ASSERT(image);

    char *saved = create_temp_path();
    ASSERT(saved);

    Carrier *carrier = figure_carrier(image);
    ASSERT(carrier);
    CHECK_CALL(bits_round_trip(carrier));

    const size_t slots = carrier->capacity(carrier);
    ASSERT_EQ(0, carrier->save(carrier, saved));
    carrier->free(carrier);

    Carrier *reopened = figure_carrier(saved);
    ASSERT(reopened);
    ASSERT_EQ(slots, reopened->capacity(reopened));

    for (size_t slot = 0; slot < slots; slot++)
        ASSERT_EQ((unsigned char)((slot * 7 + 3) % 5 < 2), reopened->read(reopened, slot));

    reopened->free(reopened);

    unlink(image);
    unlink(saved);
    free(image);
    free(saved);
    PASS();
}

TEST carrier_counts_every_colour_sample_of_a_png(void) {
    char *rgb = create_test_png(16, 8, 3);
    char *rgba = create_test_png(16, 8, 4);
    char *grey = create_test_png(16, 8, 1);
    ASSERT(rgb && rgba && grey);

    Carrier *carrier = figure_carrier(rgb);
    ASSERT(carrier);
    ASSERT_EQ((size_t)(16 * 8 * 3), carrier->capacity(carrier));
    carrier->free(carrier);

    carrier = figure_carrier(rgba);
    ASSERT(carrier);
    ASSERT_EQ((size_t)(16 * 8 * 3), carrier->capacity(carrier));
    carrier->free(carrier);

    carrier = figure_carrier(grey);
    ASSERT(carrier);
    ASSERT_EQ((size_t)(16 * 8), carrier->capacity(carrier));
    carrier->free(carrier);

    unlink(rgb);
    unlink(rgba);
    unlink(grey);
    free(rgb);
    free(rgba);
    free(grey);
    PASS();
}

TEST carrier_turns_away_what_it_cannot_carry(void) {
    const unsigned char text[] = "not an image";
    char *plain = create_temp_file(text, sizeof(text));
    ASSERT(plain);

    ASSERT_EQ(NULL, figure_carrier(plain));
    ASSERT_EQ(NULL, figure_carrier("/tmp/no_such_carrier_for_veil_tests.png"));
    ASSERT_EQ(NULL, figure_carrier(NULL));

    unlink(plain);
    free(plain);
    PASS();
}

TEST carrier_stays_inside_its_slots(void) {
    char *image = create_test_png(4, 4, 3);
    ASSERT(image);

    Carrier *carrier = figure_carrier(image);
    ASSERT(carrier);

    const size_t slots = carrier->capacity(carrier);
    ASSERT(carrier->write(carrier, slots, 1) < 0);
    ASSERT(carrier->read(carrier, slots) > 1);

    carrier->free(carrier);
    unlink(image);
    free(image);
    PASS();
}

TEST lsb_carrier_keeps_its_bits_through_a_save(void) {
    CHECK_CALL(bits_survive_a_save(create_test_png(32, 24, 3)));
    PASS();
}

TEST lsb_carrier_leaves_alpha_alone(void) {
    char *image = create_test_png(8, 8, 4);
    char *saved = create_temp_path();
    ASSERT(image && saved);

    Carrier *carrier = figure_carrier(image);
    ASSERT(carrier);

    const size_t slots = carrier->capacity(carrier);
    for (size_t slot = 0; slot < slots; slot++)
        ASSERT_EQ(0, carrier->write(carrier, slot, 1));

    ASSERT_EQ(0, carrier->save(carrier, saved));
    carrier->free(carrier);

    int width, height, channels;
    unsigned char *before = stbi_load(image, &width, &height, &channels, 0);
    unsigned char *after = stbi_load(saved, &width, &height, &channels, 0);
    ASSERT(before && after);
    ASSERT_EQ(4, channels);

    for (int pixel = 0; pixel < width * height; pixel++)
        ASSERT_EQ(before[pixel * 4 + 3], after[pixel * 4 + 3]);

    stbi_image_free(before);
    stbi_image_free(after);
    unlink(image);
    unlink(saved);
    free(image);
    free(saved);
    PASS();
}

TEST dct_carrier_keeps_its_bits_through_a_save(void) {
    CHECK_CALL(bits_survive_a_save(create_test_jpg(64, 64, 90)));
    PASS();
}

SUITE(carrier_suite) {
    RUN_TEST(carrier_counts_every_colour_sample_of_a_png);
    RUN_TEST(carrier_turns_away_what_it_cannot_carry);
    RUN_TEST(carrier_stays_inside_its_slots);
    RUN_TEST(lsb_carrier_keeps_its_bits_through_a_save);
    RUN_TEST(lsb_carrier_leaves_alpha_alone);
    RUN_TEST(dct_carrier_keeps_its_bits_through_a_save);
}
