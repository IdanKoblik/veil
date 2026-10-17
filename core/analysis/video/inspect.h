#pragma once

#include "../../codecs/video/h264.h"
#include "../image/inspect.h"
#include <stddef.h>

int video_frame_load(const struct H264Carrier *carrier, size_t index, struct PixelBuffer *out);
void video_frame_free(struct PixelBuffer *pixels);
