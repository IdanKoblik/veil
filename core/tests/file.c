#include "greatest.h"

#include "helpers.h"
#include "veil/fs/file.h"

TEST file_type_is_read_from_the_contents(void) {
    char *png = create_test_png(8, 8, 3);
    char *jpg = create_test_jpg(8, 8, 90);
    const unsigned char text[] = "plain text, not an image";
    char *plain = create_temp_file(text, sizeof(text));
    ASSERT(png && jpg && plain);

    ASSERT_EQ(TYPE_PNG_IMAGE, get_file_type(png));
    ASSERT_EQ(TYPE_JPEG_IMAGE, get_file_type(jpg));
    ASSERT_EQ(TYPE_UNKNOWN, get_file_type(plain));

    ASSERT(is_image_file(get_file_type(png)));
    ASSERT(is_image_file(get_file_type(jpg)));
    ASSERT(!is_image_file(get_file_type(plain)));

    unlink(png);
    unlink(jpg);
    unlink(plain);
    free(png);
    free(jpg);
    free(plain);
    PASS();
}

TEST file_type_accepts_any_mp4_brand_but_still_images(void) {
    const unsigned char obs[] = {0, 0, 0, 32, 'f', 't', 'y', 'p', 'i', 's', 'o', '4', 0, 0, 2, 0};
    const unsigned char dash[] = {0, 0, 0, 24, 'f', 't', 'y', 'p', 'd', 'a', 's', 'h', 0, 0, 0, 0};
    const unsigned char heic[] = {0, 0, 0, 24, 'f', 't', 'y', 'p', 'h', 'e', 'i', 'c', 0, 0, 0, 0};
    char *obs_path = create_temp_file(obs, sizeof(obs));
    char *dash_path = create_temp_file(dash, sizeof(dash));
    char *heic_path = create_temp_file(heic, sizeof(heic));
    ASSERT(obs_path && dash_path && heic_path);

    ASSERT_EQ(TYPE_MP4_VIDEO, get_file_type(obs_path));
    ASSERT_EQ(TYPE_MP4_VIDEO, get_file_type(dash_path));
    ASSERT_EQ(TYPE_UNKNOWN, get_file_type(heic_path));

    unlink(obs_path);
    unlink(dash_path);
    unlink(heic_path);
    free(obs_path);
    free(dash_path);
    free(heic_path);
    PASS();
}

TEST file_type_tells_missing_from_unknown(void) {
    ASSERT_EQ(TYPE_NOT_FOUND, get_file_type("/tmp/no_such_file_for_veil_tests"));
    ASSERT_EQ(TYPE_NOT_FOUND, get_file_type(NULL));
    ASSERT(!is_image_file(TYPE_NOT_FOUND));
    PASS();
}

TEST file_type_names_are_readable(void) {
    ASSERT_STR_EQ("PNG", file_type_name(TYPE_PNG_IMAGE));
    ASSERT_STR_EQ("JPEG", file_type_name(TYPE_JPEG_IMAGE));
    ASSERT_STR_EQ("not found", file_type_name(TYPE_NOT_FOUND));
    ASSERT_STR_EQ("unknown", file_type_name(TYPE_UNKNOWN));
    PASS();
}

TEST raw_data_round_trips_through_a_file(void) {
    char *path = create_temp_path();
    ASSERT(path);

    const unsigned char data[] = {0x00, 0xFF, 0x10, 'v', 'e', 'i', 'l', 0x00};
    ASSERT_EQ(0, write_to_file_raw_data(path, data, sizeof(data)));

    unsigned char *got = NULL;
    size_t len = 0;
    ASSERT_EQ(0, read_file_raw_data(path, &got, &len));
    ASSERT_EQ(sizeof(data), len);
    ASSERT_MEM_EQ(data, got, len);

    free(got);
    unlink(path);
    free(path);
    PASS();
}

TEST raw_data_wants_a_file_it_can_read(void) {
    unsigned char *got = NULL;
    size_t len = 0;

    ASSERT(read_file_raw_data("/tmp/no_such_file_for_veil_tests", &got, &len) < 0);
    ASSERT(write_to_file_raw_data("/tmp/no_such_dir_for_veil_tests/out", (const unsigned char *)"x", 1) < 0);
    PASS();
}

SUITE(file_suite) {
    RUN_TEST(file_type_is_read_from_the_contents);
    RUN_TEST(file_type_accepts_any_mp4_brand_but_still_images);
    RUN_TEST(file_type_tells_missing_from_unknown);
    RUN_TEST(file_type_names_are_readable);
    RUN_TEST(raw_data_round_trips_through_a_file);
    RUN_TEST(raw_data_wants_a_file_it_can_read);
}
