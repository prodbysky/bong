#ifndef STR_H_
#define STR_H_

#include <stddef.h>

// Non-null terminated owned string
typedef struct {
    char* items;
    size_t count;
    size_t capacity;
} String;

// Non-null terminated referenced string
typedef struct {
    char const* items;
    size_t count;
} StringView;

#define STR_FMT "%.*s" 
#define STR_ARG(s) (int)(s)->count, (s)->items

#endif
