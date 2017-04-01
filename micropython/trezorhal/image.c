#include <string.h>

#include "blake2s.h"
#include "ed25519-donna/ed25519.h"

#include "common.h"
#include "image.h"

bool image_parse_header(const uint8_t *data, image_header *header)
{
    if (!header) {
        image_header h;
        header = &h;
    }

    memcpy(&header->magic, data, 4);
    if (header->magic != IMAGE_MAGIC) return false;

    memcpy(&header->hdrlen, data + 4, 4);
    if (header->hdrlen != HEADER_SIZE) return false;

    memcpy(&header->expiry, data + 8, 4);
    if (header->expiry != 0) return false;

    memcpy(&header->codelen, data + 12, 4);
    if (header->hdrlen + header->codelen < 4 * 1024) return false;
    if (header->hdrlen + header->codelen > IMAGE_MAXSIZE) return false;
    if ((header->hdrlen + header->codelen) % 512 != 0) return false;

    memcpy(&header->version, data + 16, 4);

    // uint8_t reserved[427];

    memcpy(&header->sigmask, data + 0x01BF, 1);

    memcpy(header->sig, data + 0x01C0, 64);

    return true;
}

#define KEYMASK(A, B, C) ((1 << (A - 1)) | (1 << (B - 1)) | (1 << (C - 1)))

static const uint8_t *get_pubkey(uint8_t index)
{
    switch (index) {
        case KEYMASK(1, 2, 3):
            return (const uint8_t *)"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
        case KEYMASK(1, 2, 4):
            return (const uint8_t *)"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
        case KEYMASK(1, 2, 5):
            return (const uint8_t *)"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
        case KEYMASK(1, 3, 4):
            return (const uint8_t *)"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
        case KEYMASK(1, 3, 5):
            return (const uint8_t *)"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
        case KEYMASK(1, 4, 5):
            return (const uint8_t *)"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
        case KEYMASK(2, 3, 4):
            return (const uint8_t *)"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
        case KEYMASK(2, 3, 5):
            return (const uint8_t *)"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
        case KEYMASK(2, 4, 5):
            return (const uint8_t *)"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
        case KEYMASK(3, 4, 5):
            return (const uint8_t *)"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
        default:
            return NULL;
    }
}

bool image_check_signature(const uint8_t *data)
{
    image_header hdr;
    if (!image_parse_header(data, &hdr)) {
        return false;
    }

    uint8_t hash[BLAKE2S_DIGEST_LENGTH];
    BLAKE2S_CTX ctx;
    blake2s_Init(&ctx, BLAKE2S_DIGEST_LENGTH);
    blake2s_Update(&ctx, data, HEADER_SIZE - 65);
    for (int i = 0; i < 65; i++) {
        blake2s_Update(&ctx, (const uint8_t *)"\x00", 1);
    }
    blake2s_Update(&ctx, data + HEADER_SIZE, hdr.codelen);
    blake2s_Final(&ctx, hash, BLAKE2S_DIGEST_LENGTH);

    const uint8_t *pub = get_pubkey(hdr.sigmask);

    // TODO: remove debug skip of unsigned
    if (!pub) return true;
    // end

    return pub && (0 == ed25519_sign_open(hash, BLAKE2S_DIGEST_LENGTH, *(const ed25519_public_key *)pub, *(const ed25519_signature *)hdr.sig));
}

bool vendor_parse_header(const uint8_t *data, vendor_header *header)
{
    if (!header) {
        vendor_header h;
        header = &h;
    }

    memcpy(&header->magic, data, 4);
    if (header->magic != 0x565A5254) return false; // TRZV

    memcpy(&header->hdrlen, data + 4, 4);

    memcpy(&header->expiry, data + 8, 4);
    if (header->expiry != 0) return false;

    memcpy(&header->version, data + 12, 2);

    memcpy(&header->vsig_m, data + 14, 1);
    memcpy(&header->vsig_n, data + 15, 1);

    for (int i = 0; i < header->vsig_n; i++) {
        header->vpub[i] = data + 16 + i * 32;
    }
    for (int i = header->vsig_n; i < 8; i++) {
        header->vpub[i] = 0;
    }

    memcpy(&header->vstr_len, data + 16 + header->vsig_n * 32, 1);

    header->vstr = data + 16 + header->vsig_n * 32 + 1;

    header->vimg = data + 16 + header->vsig_n * 32 + 1 + header->vstr_len;
    // align to 4 bytes
    header->vimg += (-(uintptr_t)header->vimg) & 3;

    // uint8_t reserved[427];

    memcpy(&header->sigmask, data + header->hdrlen - 65, 1);

    memcpy(header->sig, data + header->hdrlen - 64, 64);

    return true;
}

bool vendor_check_signature(const uint8_t *data)
{
    vendor_header hdr;
    if (!vendor_parse_header(data, &hdr)) {
        return false;
    }

    uint8_t hash[BLAKE2S_DIGEST_LENGTH];
    BLAKE2S_CTX ctx;
    blake2s_Init(&ctx, BLAKE2S_DIGEST_LENGTH);
    blake2s_Update(&ctx, data, hdr.hdrlen - 65);
    for (int i = 0; i < 65; i++) {
        blake2s_Update(&ctx, (const uint8_t *)"\x00", 1);
    }
    blake2s_Final(&ctx, hash, BLAKE2S_DIGEST_LENGTH);

    const uint8_t *pub = get_pubkey(hdr.sigmask);

    // TODO: remove debug skip of unsigned
    if (!pub) return true;
    // end

    return pub && (0 == ed25519_sign_open(hash, BLAKE2S_DIGEST_LENGTH, *(const ed25519_public_key *)pub, *(const ed25519_signature *)hdr.sig));
}