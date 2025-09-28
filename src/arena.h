#ifndef ARENA_H_
#define ARENA_H_

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t* buffer;
    size_t capacity;
    size_t used;
} Arena;

Arena arena_new(size_t size);
void arena_free(Arena* a);
void* arena_alloc(Arena* a, size_t size);

#endif
