#pragma once

#include "../carrier.h"
#include "../codec.h"

#include <sodium.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Container magic, kept in cleartext so a reader can tell the two modes apart. */
#define HUSH_MAGIC "HUSH"
#define HUSH_MAGIC_LEN (sizeof(HUSH_MAGIC) - 1)
#define HUSH_VERSION 1

/* Header flags. */
#define HUSH_FLAG_ENCRYPTED 0x01

/* Terminates the payload when no passphrase was supplied. */
#define HUSH_END_MARKER "$$END$$"
#define HUSH_END_MARKER_LEN (sizeof(HUSH_END_MARKER) - 1)

/* magic | version | flags */
#define HUSH_PREAMBLE_PLAIN_LEN (HUSH_MAGIC_LEN + 2)
/* magic | version | flags | salt | header nonce */
#define HUSH_PREAMBLE_ENC_LEN (HUSH_PREAMBLE_PLAIN_LEN + crypto_pwhash_SALTBYTES + crypto_secretbox_NONCEBYTES)

/* codec | reserved[3] | payload_len | payload nonce */
#define HUSH_HEADER_BODY_LEN (8 + crypto_secretbox_NONCEBYTES)
#define HUSH_HEADER_SEALED_LEN (HUSH_HEADER_BODY_LEN + crypto_secretbox_MACBYTES)

/*
 * Container layout, expressed in "slots". A slot is one bit of storage handed
 * over by the carrier: the low bit of a usable pixel byte on PNG, the low bit of
 * a usable DCT coefficient on JPEG. Slot n is not always byte n of the image,
 * the carrier owns that mapping.
 *
 * Encrypted (a passphrase was supplied):
 *
 *   [ magic | version | flags | salt | nonce ]  cleartext preamble, sequential from slot 0
 *   [ secretbox(codec | len | payload nonce) ]  encrypted header, sequential
 *   [ secretbox(data) ]                         scattered over the remaining slots by the
 *                                               passphrase derived PRNG
 *
 * The preamble has to stay in the clear: without the salt and the nonce there is
 * no way to re-derive the key and open the header. Everything that describes the
 * payload (its length, where the scatter walk starts from) lives in the sealed
 * header instead.
 *
 * Plain (no passphrase):
 *
 *   [ magic | version | flags ]                 cleartext preamble, sequential from slot 0
 *   [ data ][ $$END$$ ]                         sequential, the marker ends the payload
 */

/* Offsets inside the (sealed) header body. */
#define HUSH_BODY_CODEC_OFF 0
#define HUSH_BODY_LEN_OFF 4
#define HUSH_BODY_NONCE_OFF 8

int container_embed(const Carrier *carrier, enum CodecType codec, const char *passphrase, const unsigned char *data, size_t data_len);
int container_extract(const Carrier *carrier, const char *passphrase, enum CodecType *codec, unsigned char **data, size_t *data_len);

#ifdef __cplusplus
}
#endif
