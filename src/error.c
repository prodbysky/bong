#include "error.h"
#include <stdio.h>

Location get_loc(const SourceFile* file, size_t offset) {
    Location l = {.line = 1, .col = 1};
    for (size_t i = 0; i < offset && i < file->content.count; i++) {
        if (file->content.items[i] == '\n') {
            l.line += 1;
            l.col = 1;
        } else {
            l.col++;
        }
    }
    return l;
}

ptrdiff_t get_line_begin(const SourceFile* file, size_t offset) {
    ptrdiff_t result = 0;
    for (size_t i = 0; i < offset && i < file->content.count; i++) {
        if (file->content.items[i] == '\n') result = i;
    }
    return result;
}

ptrdiff_t get_line_end(const SourceFile* file, size_t offset) {
    for (size_t i = file->content.count - 1; i > offset && i > 0; i--) {
        if (file->content.items[i] == '\n') return i;
    }
#ifdef DEBUG
    fprintf(stderr, "[DEBUG]: Tried to get invalid end of line offset in %s with offset %zu\n", file->name, offset);
#endif
    return -1;
}

void bong_error(const SourceFile* source, size_t begin) {
    Location loc = get_loc(source, begin);
    fprintf(stderr, "./%s:%zu:%zu\n", source->name, loc.line, loc.col);
    ptrdiff_t l_begin = get_line_begin(source, begin);
    fprintf(stderr, "%.*s\n", (int)(get_line_end(source, begin) - l_begin), &source->content.items[l_begin]);
}
