#include "greatest.h"

#include "helpers.h"
#include "veil/analysis/stream.h"
#include "veil/analysis/video/inspect.h"

TEST video_lsb_stream_takes_the_carrier_bits(void) {
    char *video = create_test_mp4(32, 24, 3);
    if (!video)
        SKIPm("ffmpeg with libx264 is not available");

    struct H264Carrier *carrier = h264_carrier_init(video);
    ASSERT(carrier);

    const size_t slots = carrier->slots;
    unsigned char *bits = malloc(slots);
    ASSERT(bits);
    for (size_t slot = 0; slot < slots; slot++)
        bits[slot] = carrier->carrier.read(&carrier->carrier, slot);

    struct Stream stream;
    ASSERT_EQ(0, stream_take_h264(carrier, &stream));
    ASSERT_EQ(slots, stream.slots);
    ASSERT_EQ(slots / 8, stream.len);

    for (size_t slot = 0; slot < stream.len * 8; slot++)
        ASSERT_EQ(bits[slot], (stream.bytes[slot / 8] >> (slot % 8)) & 1);

    // The map moved, so the carrier has nothing left to read, write or take again.
    ASSERT(carrier->carrier.read(&carrier->carrier, 0) > 1);
    ASSERT(carrier->carrier.write(&carrier->carrier, 0, 1) < 0);
    struct Stream again;
    ASSERT(stream_take_h264(carrier, &again) < 0);

    struct Stream loaded;
    ASSERT_EQ(0, stream_load(video, STREAM_LSB, &loaded));
    ASSERT_EQ(stream.len, loaded.len);
    ASSERT_MEM_EQ(stream.bytes, loaded.bytes, stream.len);

    free(bits);
    stream_free(&stream);
    stream_free(&loaded);
    carrier->carrier.free(&carrier->carrier);
    unlink(video);
    free(video);
    PASS();
}

TEST video_frames_load_as_rgb(void) {
    char *video = create_test_mp4(32, 24, 3);
    if (!video)
        SKIPm("ffmpeg with libx264 is not available");

    struct H264Carrier *carrier = h264_carrier_init(video);
    ASSERT(carrier);

    struct PixelBuffer pixels = {0};
    ASSERT_EQ(0, video_frame_load(carrier, carrier->frame_count - 1, &pixels));
    ASSERT_EQ(32, pixels.width);
    ASSERT_EQ(24, pixels.height);
    ASSERT_EQ(3, pixels.channels);
    ASSERT_EQ((size_t)(32 * 24 * 3), pixels.len);
    video_frame_free(&pixels);

    ASSERT(video_frame_load(carrier, carrier->frame_count, &pixels) < 0);

    carrier->carrier.free(&carrier->carrier);
    unlink(video);
    free(video);
    PASS();
}

TEST video_frames_seek_to_the_right_frame(void) {
    char *video = create_temp_path();
    char *expected = create_temp_path();
    ASSERT(video && expected);

    char command[1024];
    snprintf(command, sizeof(command),
             "ffmpeg -loglevel error -y -f lavfi -i testsrc=size=32x24:rate=10 -frames:v 12 -g 4 -bf 2 "
             "-c:v libx264 -pix_fmt yuv420p -f mp4 %s && "
             "ffmpeg -loglevel error -y -i %s -sws_flags bilinear -f rawvideo -pix_fmt rgb24 %s",
             video, video, expected);
    if (system(command) != 0)
        SKIPm("ffmpeg with libx264 is not available");

    FILE *file = fopen(expected, "rb");
    ASSERT(file);
    unsigned char frames[12 * 32 * 24 * 3];
    ASSERT_EQ(sizeof(frames), fread(frames, 1, sizeof(frames), file));
    fclose(file);

    struct H264Carrier *carrier = h264_carrier_init(video);
    ASSERT(carrier);
    ASSERT_EQ((size_t)12, carrier->frame_count);

    // Out of order, so every load has to seek rather than ride on the previous position.
    static const size_t order[] = {11, 0, 6, 3, 9, 1, 10, 4, 7, 2, 8, 5};
    for (size_t i = 0; i < 12; i++) {
        struct PixelBuffer pixels = {0};
        ASSERT_EQ(0, video_frame_load(carrier, order[i], &pixels));
        ASSERT_MEM_EQ(frames + order[i] * 32 * 24 * 3, pixels.samples, pixels.len);
        video_frame_free(&pixels);
    }

    carrier->carrier.free(&carrier->carrier);
    unlink(video);
    unlink(expected);
    free(video);
    free(expected);
    PASS();
}

TEST video_slots_are_the_luma_lsbs(void) {
    char *video = create_test_mp4(30, 22, 4);
    char *raw = create_temp_path();
    if (!video)
        SKIPm("ffmpeg with libx264 is not available");
    ASSERT(raw);

    // An odd width keeps rows off byte boundaries, so the packed and bit-by-bit paths both run.
    char command[512];
    snprintf(command, sizeof(command), "ffmpeg -loglevel error -y -i %s -f rawvideo -pix_fmt yuv420p %s", video, raw);
    ASSERT_EQ(0, system(command));

    const size_t luma = 30 * 22;
    const size_t frame_len = luma + 2 * (15 * 11);
    unsigned char frames[4 * (30 * 22 + 2 * 15 * 11)];

    FILE *file = fopen(raw, "rb");
    ASSERT(file);
    ASSERT_EQ(sizeof(frames), fread(frames, 1, sizeof(frames), file));
    fclose(file);

    struct H264Carrier *carrier = h264_carrier_init(video);
    ASSERT(carrier);
    ASSERT_EQ(4 * luma, carrier->slots);

    for (size_t slot = 0; slot < carrier->slots; slot++)
        ASSERT_EQ(frames[(slot / luma) * frame_len + slot % luma] & 1, carrier->carrier.read(&carrier->carrier, slot));

    carrier->carrier.free(&carrier->carrier);
    unlink(video);
    unlink(raw);
    free(video);
    free(raw);
    PASS();
}

SUITE(video_suite) {
    RUN_TEST(video_slots_are_the_luma_lsbs);
    RUN_TEST(video_lsb_stream_takes_the_carrier_bits);
    RUN_TEST(video_frames_load_as_rgb);
    RUN_TEST(video_frames_seek_to_the_right_frame);
}
