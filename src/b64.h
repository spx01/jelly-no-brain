#pragma once

#include <stdbool.h>
#include <stddef.h>

static inline size_t b64_encoded_size(size_t size) {
    return (((size) + 2) / 3 * 4);
}

size_t b64_decoded_size(const char *str);

void b64_encode(char *dest, const void *src, size_t size);
char *b64_encode_alloc(const void *src, size_t size);

bool b64_decode(void *dest, const char *src);
void *b64_decode_alloc(const char *src, size_t *dest_size);
