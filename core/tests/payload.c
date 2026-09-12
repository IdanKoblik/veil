#include "greatest.h"

#include <sodium.h>
#include <string.h>

#include "veil/crypto/payload.h"

#define MESSAGE "the payload begins here"

static void fill(unsigned char *out, size_t len, unsigned char with) {
    memset(out, with, len);
}

TEST payload_grows_by_the_tag(void) {
    unsigned char nonce[NONCE_LEN];
    unsigned char key[KEY_LEN];

    fill(nonce, sizeof(nonce), 0x01);
    fill(key, sizeof(key), 0x02);

    struct Payload *payload = encrypt_data((const unsigned char *)MESSAGE, strlen(MESSAGE), nonce, key);
    ASSERT(payload != NULL);
    ASSERT_EQ(strlen(MESSAGE) + crypto_secretbox_MACBYTES, payload->body_len);

    /* Whatever else it is, it is not the message in the clear. */
    ASSERT(memcmp(payload->body, MESSAGE, strlen(MESSAGE)) != 0);

    ASSERT_EQ(0, payload_free(payload));
    PASS();
}

TEST payload_opens_with_the_same_key(void) {
    unsigned char nonce[NONCE_LEN];
    unsigned char key[KEY_LEN];

    fill(nonce, sizeof(nonce), 0x03);
    fill(key, sizeof(key), 0x04);

    struct Payload *payload = encrypt_data((const unsigned char *)MESSAGE, strlen(MESSAGE), nonce, key);
    ASSERT(payload != NULL);

    unsigned char opened[sizeof(MESSAGE)] = {0};
    ASSERT_EQ(0, crypto_secretbox_open_easy(opened, payload->body, payload->body_len, nonce, key));
    ASSERT_EQ(0, memcmp(opened, MESSAGE, strlen(MESSAGE)));

    payload_free(payload);
    PASS();
}

TEST payload_stays_shut_for_another_key(void) {
    unsigned char nonce[NONCE_LEN];
    unsigned char key[KEY_LEN];
    unsigned char wrong[KEY_LEN];

    fill(nonce, sizeof(nonce), 0x05);
    fill(key, sizeof(key), 0x06);
    fill(wrong, sizeof(wrong), 0x07);

    struct Payload *payload = encrypt_data((const unsigned char *)MESSAGE, strlen(MESSAGE), nonce, key);
    ASSERT(payload != NULL);

    unsigned char opened[sizeof(MESSAGE)] = {0};
    ASSERT(crypto_secretbox_open_easy(opened, payload->body, payload->body_len, nonce, wrong) != 0);

    payload_free(payload);
    PASS();
}

TEST payload_follows_the_nonce(void) {
    unsigned char key[KEY_LEN];
    unsigned char first[NONCE_LEN];
    unsigned char second[NONCE_LEN];

    fill(key, sizeof(key), 0x08);
    fill(first, sizeof(first), 0x09);
    fill(second, sizeof(second), 0x0A);

    struct Payload *one = encrypt_data((const unsigned char *)MESSAGE, strlen(MESSAGE), first, key);
    struct Payload *two = encrypt_data((const unsigned char *)MESSAGE, strlen(MESSAGE), second, key);

    ASSERT(one != NULL);
    ASSERT(two != NULL);
    ASSERT_EQ(one->body_len, two->body_len);
    ASSERT(memcmp(one->body, two->body, one->body_len) != 0);

    payload_free(one);
    payload_free(two);
    PASS();
}

TEST payload_free_takes_nothing(void) {
    ASSERT(payload_free(NULL) != 0);
    PASS();
}

SUITE(payload_suite) {
    RUN_TEST(payload_grows_by_the_tag);
    RUN_TEST(payload_opens_with_the_same_key);
    RUN_TEST(payload_stays_shut_for_another_key);
    RUN_TEST(payload_follows_the_nonce);
    RUN_TEST(payload_free_takes_nothing);
}
