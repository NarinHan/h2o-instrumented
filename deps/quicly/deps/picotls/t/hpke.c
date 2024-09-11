#ifndef LOGGING_H
#define LOGGING_H
#include "logging.h"
#endif

/*
 * Copyright (c) 2022 Fastly, Kazuho Oku
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#ifdef _WINDOWS
#include "wincompat.h"
#endif
#include <assert.h>
#include <string.h>
#include "picotls.h"
#include "../deps/picotest/picotest.h"
#include "test.h"
#include "../lib/hpke.c"

static ptls_hpke_kem_t *test_kem;
static ptls_hpke_cipher_suite_t *test_cipher;

void test_one_hpke(void)
{
    static const uint8_t cleartext[] = {0x42, 0x65, 0x61, 0x75, 0x74, 0x79, 0x20, 0x69, 0x73, 0x20, 0x74, 0x72, 0x75, 0x74, 0x68,
                                        0x2c, 0x20, 0x74, 0x72, 0x75, 0x74, 0x68, 0x20, 0x62, 0x65, 0x61, 0x75, 0x74, 0x79},
                         info[] = {0x4f, 0x64, 0x65, 0x20, 0x6f, 0x6e, 0x20, 0x61, 0x20, 0x47,
                                   0x72, 0x65, 0x63, 0x69, 0x61, 0x6e, 0x20, 0x55, 0x72, 0x6e},
                         aad[2][7] = {{0x43, 0x6f, 0x75, 0x6e, 0x74, 0x2d, 0x30}, {0x43, 0x6f, 0x75, 0x6e, 0x74, 0x2d, 0x31}};
    static const struct {
        struct {
            uint16_t kem;
            uint16_t kdf;
            uint16_t aead;
        } id;
        struct {
            uint8_t bytes[65];
            size_t len;
        } server_pubkey, client_pubkey, dh;
        uint8_t expected_secret[PTLS_MAX_DIGEST_SIZE];
        uint8_t expected_ciphertext[2][sizeof(cleartext) + 32 /* max tag size */];
    } all[] =
        {{.id = {PTLS_HPKE_KEM_X25519_SHA256, PTLS_HPKE_HKDF_SHA256, PTLS_HPKE_AEAD_AES_128_GCM},
          .server_pubkey = {{0x39, 0x48, 0xcf, 0xe0, 0xad, 0x1d, 0xdb, 0x69, 0x5d, 0x78, 0x0e, 0x59, 0x07, 0x71, 0x95, 0xda,
                             0x6c, 0x56, 0x50, 0x6b, 0x02, 0x73, 0x29, 0x79, 0x4a, 0xb0, 0x2b, 0xca, 0x80, 0x81, 0x5c, 0x4d},
                            32},
          .client_pubkey = {{0x37, 0xfd, 0xa3, 0x56, 0x7b, 0xdb, 0xd6, 0x28, 0xe8, 0x86, 0x68, 0xc3, 0xc8, 0xd7, 0xe9, 0x7d,
                             0x1d, 0x12, 0x53, 0xb6, 0xd4, 0xea, 0x6d, 0x44, 0xc1, 0x50, 0xf7, 0x41, 0xf1, 0xbf, 0x44, 0x31},
                            32},
          .dh = {{0xb3, 0xb5, 0xc1, 0x9e, 0xab, 0x3f, 0x08, 0x8a, 0xc1, 0x8f, 0x23, 0xf7, 0x74, 0xff, 0x64, 0x14,
                  0xba, 0x4f, 0xde, 0x45, 0x40, 0x4d, 0x10, 0x08, 0x5e, 0xfc, 0x3e, 0x4d, 0xc9, 0xc7, 0x2e, 0x35},
                 32},
          .expected_secret = {0xfe, 0x0e, 0x18, 0xc9, 0xf0, 0x24, 0xce, 0x43, 0x79, 0x9a, 0xe3, 0x93, 0xc7, 0xe8, 0xfe, 0x8f,
                              0xce, 0x9d, 0x21, 0x88, 0x75, 0xe8, 0x22, 0x7b, 0x01, 0x87, 0xc0, 0x4e, 0x7d, 0x2e, 0xa1, 0xfc},
          .expected_ciphertext = {{0xf9, 0x38, 0x55, 0x8b, 0x5d, 0x72, 0xf1, 0xa2, 0x38, 0x10, 0xb4, 0xbe, 0x2a, 0xb4, 0xf8,
                                   0x43, 0x31, 0xac, 0xc0, 0x2f, 0xc9, 0x7b, 0xab, 0xc5, 0x3a, 0x52, 0xae, 0x82, 0x18, 0xa3,
                                   0x55, 0xa9, 0x6d, 0x87, 0x70, 0xac, 0x83, 0xd0, 0x7b, 0xea, 0x87, 0xe1, 0x3c, 0x51, 0x2a},
                                  {0xaf, 0x2d, 0x7e, 0x9a, 0xc9, 0xae, 0x7e, 0x27, 0x0f, 0x46, 0xba, 0x1f, 0x97, 0x5b, 0xe5,
                                   0x3c, 0x09, 0xf8, 0xd8, 0x75, 0xbd, 0xc8, 0x53, 0x54, 0x58, 0xc2, 0x49, 0x4e, 0x8a, 0x6e,
                                   0xab, 0x25, 0x1c, 0x03, 0xd0, 0xc2, 0x2a, 0x56, 0xb8, 0xca, 0x42, 0xc2, 0x06, 0x3b, 0x84}}},
         {.id = {PTLS_HPKE_KEM_P256_SHA256, PTLS_HPKE_HKDF_SHA256, PTLS_HPKE_AEAD_AES_128_GCM},
          .server_pubkey = {{0x04, 0xfe, 0x8c, 0x19, 0xce, 0x09, 0x05, 0x19, 0x1e, 0xbc, 0x29, 0x8a, 0x92, 0x45, 0x79, 0x25, 0x31,
                             0xf2, 0x6f, 0x0c, 0xec, 0xe2, 0x46, 0x06, 0x39, 0xe8, 0xbc, 0x39, 0xcb, 0x7f, 0x70, 0x6a, 0x82, 0x6a,
                             0x77, 0x9b, 0x4c, 0xf9, 0x69, 0xb8, 0xa0, 0xe5, 0x39, 0xc7, 0xf6, 0x2f, 0xb3, 0xd3, 0x0a, 0xd6, 0xaa,
                             0x8f, 0x80, 0xe3, 0x0f, 0x1d, 0x12, 0x8a, 0xaf, 0xd6, 0x8a, 0x2c, 0xe7, 0x2e, 0xa0},
                            65},
          .client_pubkey = {{0x04, 0xa9, 0x27, 0x19, 0xc6, 0x19, 0x5d, 0x50, 0x85, 0x10, 0x4f, 0x46, 0x9a, 0x8b, 0x98, 0x14, 0xd5,
                             0x83, 0x8f, 0xf7, 0x2b, 0x60, 0x50, 0x1e, 0x2c, 0x44, 0x66, 0xe5, 0xe6, 0x7b, 0x32, 0x5a, 0xc9, 0x85,
                             0x36, 0xd7, 0xb6, 0x1a, 0x1a, 0xf4, 0xb7, 0x8e, 0x5b, 0x7f, 0x95, 0x1c, 0x09, 0x00, 0xbe, 0x86, 0x3c,
                             0x40, 0x3c, 0xe6, 0x5c, 0x9b, 0xfc, 0xb9, 0x38, 0x26, 0x57, 0x22, 0x2d, 0x18, 0xc4},
                            65},
          .dh = {{0x13, 0xf9, 0x18, 0x52, 0x94, 0x58, 0xd2, 0x54, 0x25, 0x31, 0x40, 0x68, 0x88, 0xc8, 0xa6, 0xd4,
                  0xea, 0x7f, 0xf4, 0x73, 0xa6, 0xf4, 0xdb, 0x45, 0x2a, 0xc3, 0xc4, 0xae, 0x1d, 0x01, 0xce, 0xa1},
                 32},
          .expected_secret = {0xc0, 0xd2, 0x6a, 0xea, 0xb5, 0x36, 0x60, 0x9a, 0x57, 0x2b, 0x07, 0x69, 0x5d, 0x93, 0x3b, 0x58,
                              0x9d, 0xcf, 0x36, 0x3f, 0xf9, 0xd9, 0x3c, 0x93, 0xad, 0xea, 0x53, 0x7a, 0xea, 0xbb, 0x8c, 0xb8},
          .expected_ciphertext = {{0x5a, 0xd5, 0x90, 0xbb, 0x8b, 0xaa, 0x57, 0x7f, 0x86, 0x19, 0xdb, 0x35, 0xa3, 0x63, 0x11,
                                   0x22, 0x6a, 0x89, 0x6e, 0x73, 0x42, 0xa6, 0xd8, 0x36, 0xd8, 0xb7, 0xbc, 0xd2, 0xf2, 0x0b,
                                   0x6c, 0x7f, 0x90, 0x76, 0xac, 0x23, 0x2e, 0x3a, 0xb2, 0x52, 0x3f, 0x39, 0x51, 0x34, 0x34},
                                  {0xfa, 0x6f, 0x03, 0x7b, 0x47, 0xfc, 0x21, 0x82, 0x6b, 0x61, 0x01, 0x72, 0xca, 0x96, 0x37,
                                   0xe8, 0x2d, 0x6e, 0x58, 0x01, 0xeb, 0x31, 0xcb, 0xd3, 0x74, 0x82, 0x71, 0xaf, 0xfd, 0x4e,
                                   0xcb, 0x06, 0x64, 0x6e, 0x03, 0x29, 0xcb, 0xdf, 0x3c, 0x3c, 0xd6, 0x55, 0xb2, 0x8e, 0x82}}},
         {.id = {PTLS_HPKE_KEM_P256_SHA256, PTLS_HPKE_HKDF_SHA512, PTLS_HPKE_AEAD_AES_128_GCM},
          .server_pubkey = {{0x04, 0x08, 0x5a, 0xa5, 0xb6, 0x65, 0xdc, 0x38, 0x26, 0xf9, 0x65, 0x0c, 0xcb, 0xcc, 0x47, 0x1b, 0xe2,
                             0x68, 0xc8, 0xad, 0xa8, 0x66, 0x42, 0x2f, 0x73, 0x9e, 0x2d, 0x53, 0x1d, 0x4a, 0x88, 0x18, 0xa9, 0x46,
                             0x6b, 0xc6, 0xb4, 0x49, 0x35, 0x70, 0x96, 0x23, 0x29, 0x19, 0xec, 0x4f, 0xe9, 0x07, 0x0c, 0xcb, 0xac,
                             0x4a, 0xac, 0x30, 0xf4, 0xa1, 0xa5, 0x3e, 0xfc, 0xf7, 0xaf, 0x90, 0x61, 0x0e, 0xdd},
                            65},
          .client_pubkey = {{0x04, 0x93, 0xed, 0x86, 0x73, 0x5b, 0xdf, 0xb9, 0x78, 0xcc, 0x05, 0x5c, 0x98, 0xb4, 0x56, 0x95, 0xad,
                             0x7c, 0xe6, 0x1c, 0xe7, 0x48, 0xf4, 0xdd, 0x63, 0xc5, 0x25, 0xa3, 0xb8, 0xd5, 0x3a, 0x15, 0x56, 0x5c,
                             0x68, 0x97, 0x88, 0x80, 0x70, 0x07, 0x0c, 0x15, 0x79, 0xdb, 0x1f, 0x86, 0xaa, 0xa5, 0x6d, 0xeb, 0x82,
                             0x97, 0xe6, 0x4d, 0xb7, 0xe8, 0x92, 0x4e, 0x72, 0x86, 0x6f, 0x9a, 0x47, 0x25, 0x80},
                            65},
          .dh = {{0x00, 0x63, 0x70, 0x63, 0x7d, 0xb3, 0x7e, 0xf6, 0x8f, 0x3a, 0x55, 0x0b, 0x9a, 0xba, 0xb6, 0xa4,
                  0xb9, 0xa3, 0x4a, 0x16, 0x8f, 0x34, 0x29, 0x26, 0xda, 0x14, 0x25, 0xa1, 0x68, 0x49, 0xa0, 0x95},
                 32},
          .expected_secret = {0x02, 0xf5, 0x84, 0x73, 0x63, 0x90, 0xfc, 0x93, 0xf5, 0xb4, 0xad, 0x03, 0x98, 0x26, 0xa3, 0xfa,
                              0x08, 0xe9, 0x91, 0x1b, 0xd1, 0x21, 0x5a, 0x3d, 0xb8, 0xe8, 0x79, 0x1b, 0xa5, 0x33, 0xca, 0xfd},
          .expected_ciphertext = {{0xd3, 0xcf, 0x49, 0x84, 0x93, 0x14, 0x84, 0xa0, 0x80, 0xf7, 0x4c, 0x1b, 0xb2, 0xa6, 0x78,
                                   0x27, 0x00, 0xdc, 0x1f, 0xef, 0x9a, 0xbe, 0x84, 0x42, 0xe4, 0x4a, 0x6f, 0x09, 0x04, 0x4c,
                                   0x88, 0x90, 0x72, 0x00, 0xb3, 0x32, 0x00, 0x35, 0x43, 0x75, 0x4e, 0xb5, 0x19, 0x17, 0xba},
                                  {0xd1, 0x44, 0x14, 0x55, 0x5a, 0x47, 0x26, 0x9d, 0xfe, 0xad, 0x9f, 0xbf, 0x26, 0xab, 0xb3,
                                   0x03, 0x36, 0x5e, 0x40, 0x70, 0x9a, 0x4e, 0xd1, 0x6e, 0xae, 0xfe, 0x1f, 0x20, 0x70, 0xf1,
                                   0xdd, 0xeb, 0x1b, 0xdd, 0x94, 0xd9, 0xe4, 0x11, 0x86, 0xf1, 0x24, 0xe0, 0xac, 0xc6, 0x2d}}},
         {{0}}},
      *test;
    int ret;

    /* find the corresponding test vector or bail out if not found */
    for (test = all;
         !(test->id.kem == test_kem->id && test->id.kdf == test_cipher->id.kdf && test->id.aead == test_cipher->id.aead); ++test) {
        if (test->id.kem == 0) {
            note("no test vector for given kem / cipher");
            return;
        }
    }

    { /* derivation from DH shared secret */
        uint8_t secret[PTLS_MAX_DIGEST_SIZE];
        ret = dh_derive(test_kem, secret, ptls_iovec_init(test->client_pubkey.bytes, test->client_pubkey.len),
                        ptls_iovec_init(test->server_pubkey.bytes, test->server_pubkey.len),
                        ptls_iovec_init(test->dh.bytes, test->dh.len));
        ok(ret == 0);
        ok(memcmp(secret, test->expected_secret, test_kem->hash->digest_size) == 0);
    }

    { /* encryption */
        ptls_aead_context_t *enc;
        uint8_t ciphertext[sizeof(cleartext) + 32];
        ret = key_schedule(test_kem, test_cipher, &enc, 1, test->expected_secret, ptls_iovec_init(info, sizeof(info)));
        ok(ret == 0);
        for (uint64_t seq = 0; seq < 2; ++seq) {
            ptls_aead_encrypt(enc, ciphertext, cleartext, sizeof(cleartext), seq, aad[seq], sizeof(aad[seq]));
            ok(memcmp(ciphertext, test->expected_ciphertext[seq], sizeof(cleartext) + test_cipher->aead->tag_size) == 0);
        }
        ptls_aead_free(enc);
    }

    { /* decryption */
        ptls_aead_context_t *dec;
        uint8_t text_recovered[sizeof(cleartext)];
        ret = key_schedule(test_kem, test_cipher, &dec, 0, test->expected_secret, ptls_iovec_init(info, sizeof(info)));
        ok(ret == 0);
        for (uint64_t seq = 0; seq < 2; ++seq) {
            ok(ptls_aead_decrypt(dec, text_recovered, test->expected_ciphertext[seq],
                                 sizeof(text_recovered) + test_cipher->aead->tag_size, seq, aad[seq],
                                 sizeof(aad[seq])) == sizeof(cleartext));
            ok(memcmp(text_recovered, cleartext, sizeof(cleartext)) == 0);
        }
        ptls_aead_free(dec);
    }
}

void test_hpke(ptls_hpke_kem_t **all_kems, ptls_hpke_cipher_suite_t **all_ciphers)
{

    for (ptls_hpke_kem_t **kem = all_kems; *kem != NULL; ++kem) {
        for (ptls_hpke_cipher_suite_t **cipher = all_ciphers; *cipher != NULL; ++cipher) {
            char namebuf[64];
            snprintf(namebuf, sizeof(namebuf), "%s-%s/%s-%s", (*kem)->keyex->name, (*kem)->hash->name, (*cipher)->hash->name,
                     (*cipher)->aead->name);
            test_kem = *kem;
            test_cipher = *cipher;
            subtest(namebuf, test_one_hpke);
        }
    }
}
