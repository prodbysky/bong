#include "arena.h"
#include <string.h>

#define DA_INIT_CAP 16
#define da_push(arr, item, arena) do { \
    if ((arr)->capacity == 0) {\
        (arr)->capacity = DA_INIT_CAP;\
        (arr)->items = arena_alloc((arena), sizeof(*(arr)->items) * (arr)->capacity);\
    }\
    if ((arr)->count >= (arr)->capacity) {\
        (arr)->capacity *= 1.5; \
        void* old = (arr)->items; \
        (arr)->items = arena_alloc((arena), sizeof(*(arr)->items) * (arr)->capacity);\
        memcpy((arr)->items, old, sizeof(*(arr)->items) * ((arr)->capacity / 1.5)); \
    }\
    (arr)->items[(arr)->count++] = (item);\
} while (false)
