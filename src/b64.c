#include "b64.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

const char *const B64_TABLE =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void b64_encode(char *dest, const void *src, size_t size) {
    size_t lim = size + (size % 3 == 0 ? 0 : 3 - (size % 3));
    for (size_t i = 0, chunk = 0; i < lim; i += 3, ++chunk) {
        uint8_t bytes[3] = {0};
        memcpy(bytes, src + i, MIN(3, size - i));
        dest[chunk * 4 + 0] = B64_TABLE[bytes[0] >> 2];
        dest[chunk * 4 + 1] =
            B64_TABLE[((bytes[0] & 0x3) << 4) | (bytes[1] >> 4)];
        dest[chunk * 4 + 2] =
            B64_TABLE[((bytes[1] & 0xf) << 2) | (bytes[2] >> 6)];
        dest[chunk * 4 + 3] = B64_TABLE[bytes[2] & 0x3f];
    }
    size_t out_size = b64_encoded_size(size);
    switch (size % 3) {
    case 1:
        dest[out_size - 2] = '=';
    case 2:
        dest[out_size - 1] = '=';
    }
    dest[out_size] = '\0';
}

char *b64_encode_alloc(const void *src, size_t size) {
    char *res = malloc(b64_encoded_size(size) + 1);
    if (!res)
        return NULL;
    b64_encode(res, src, size);
    return res;
}

size_t b64_decoded_size(const char *str) {
    size_t size = strlen(str);
    if (size % 4 != 0)
        return 0;
    return size / 4 * 3 - (str[size - 1] == '=') - (str[size - 2] == '=');
}

bool b64_decode(void *dest, const char *src) {
    size_t in_size = strlen(src);
    if (in_size % 4 != 0)
        return false;
    uint8_t *out = dest;

    for (size_t i = 0, chunk = 0; i < in_size; i += 4, ++chunk) {
        uint8_t data[4];
        for (int j = 0; j < 4; ++j) {
            data[j] = strchr(B64_TABLE, src[i + j]) - B64_TABLE;
        }
        out[chunk * 3 + 0] = (data[0] << 2) | (data[1] >> 4);
        if (src[i + 2] == '=')
            break;
        out[chunk * 3 + 1] = (data[1] << 4) | (data[2] >> 2);
        if (src[i + 3] == '=')
            break;
        out[chunk * 3 + 2] = (data[2] << 6) | (data[3] >> 0);
    }
    return true;
}

void *b64_decode_alloc(const char *src, size_t *dest_size) {
    size_t tmp;
    if (dest_size == NULL) {
        dest_size = &tmp;
    }
    *dest_size = b64_decoded_size(src);
    if (!*dest_size)
        return NULL;
    void *res = malloc(*dest_size);
    if (!res)
        return NULL;
    if (!b64_decode(res, src)) {
        free(res);
        return NULL;
    }
    return res;
}