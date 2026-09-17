#include "carrier.h"
#include "../fs/file.h"
#include "image/jpeg.h"
#include "image/lossless.h"
#include "video/h264.h"
#include <veil/log.h>

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
    case TYPE_MP4_VIDEO: {
        struct H264Carrier *h264 = h264_carrier_init(target);
        if (!h264)
            return NULL;

        return &h264->carrier;
    }
    default:
        ERROR("No carrier supports %s files (%s)", file_type_name(file_type), target);
        return NULL;
    }
}
