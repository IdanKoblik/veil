#include "carrier.h"
#include "../fs/file.h"
#include "image/jpeg.h"
#include "image/lossless.h"
#ifdef VEIL_WITH_VIDEO
#include "video/h264.h"
#endif
#include <sodium/randombytes.h>
#include <veil/log.h>

void lsb_matching(unsigned char *sample, unsigned char bit) {
    unsigned char value = *sample;
    if ((value & 1) == bit)
        return;

    switch (value) {
    case 0: {
        value = 1;
        break;
    }
    case 255: {
        value = 254;
        break;
    }
    default:
        value += randombytes_uniform(2) ? 1 : -1;
    }

    *sample = value;
}

Carrier *figure_carrier(const char *target) {
    if (!target)
        return NULL;

    const enum FileType file_type = get_file_type(target);
    DEBUG("Picking a carrier for %s (%s)", target, file_type_name(file_type));
    switch (file_type) {
    case TYPE_PNG_IMAGE: {
        struct LosslessCarrier *ll = lossless_carrier_init(target);
        if (!ll)
            return NULL;

        return &ll->carrier;
    }
    case TYPE_JPEG_IMAGE: {
        struct JpegCarrier *jpeg = jpeg_carrier_init(target);
        if (!jpeg)
            return NULL;

        return &jpeg->carrier;
    }
#ifdef VEIL_WITH_VIDEO
    case TYPE_MP4_VIDEO: {
        struct H264Carrier *h264 = h264_carrier_init(target);
        if (!h264)
            return NULL;

        return &h264->carrier;
    }
#endif
    default:
        ERROR("No carrier supports %s files (%s)", file_type_name(file_type), target);
        return NULL;
    }
}
