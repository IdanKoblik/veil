#include "carrier.h"
#include "../fs/file.h"
#include "image/dct.h"
#include "image/lsb.h"
#include <veil/log.h>

Carrier *figure_carrier(const char* target) {
    if (!target)
        return NULL;

    const enum FileType file_type = get_file_type(target);
    DEBUG("Picking a carrier for %s (%s)", target, file_type_name(file_type));
    switch (file_type) {
    case TYPE_PNG_IMAGE:
        {
            struct LsbCarrier *lsb = lsb_carrier_init(target);
            if (!lsb)
                return NULL;

            return &lsb->carrier;
        }
    case TYPE_JPEG_IMAGE:
        {
            struct DctCarrier *dct = dct_carrier_init(target);
            if (!dct)
                return NULL;

            return &dct->carrier;
        }
    default:
        ERROR("No carrier supports %s files (%s)", file_type_name(file_type), target);
        return NULL;
    }
}
