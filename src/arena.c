#include "arena.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

Arena arena_new(size_t size) {
    Arena a = {0};
    a.capacity = size;
    a.buffer = calloc(size, sizeof(uint8_t));
    assert(a.buffer && "Ran out of RAM?");
    return a;
}

void* arena_alloc(Arena* a, size_t size) {
    assert(a->buffer);
    assert(a->used + size < a->capacity);
#ifdef DEBUG
    fprintf(stderr, "[DEBUG]: Allocated %zu bytes when %zu is available\n", size, a->capacity - a->used);
#endif
    void* buf = a->buffer + a->used;
    a->used += size;
    return buf;
}

void arena_free(Arena* a) {
    assert(a->buffer);
    free(a->buffer);
    memset(a, 0, sizeof(Arena));
}
