#include "carrier.h"
#include "../fs/file.h"
#include "image/lsb.h"

inline Carrier *figure_carrier(const char* target) {
    if (!target)
        return NULL;

    const enum FileType file_type = get_file_type(target);
    switch (file_type) {
    case TYPE_PNG_IMAGE:
        {
            struct LsbCarrier *lsb = lsb_carrier_init(target);
            if (!lsb)
                return NULL;

            return &lsb->carrier;
        }
    default:
        return NULL;
    }
}
