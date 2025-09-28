#include "fs.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

bool read_entire_file(const char* path, SourceFile* f, Arena* arena) {
    assert(path);
    f->name = path;
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "[ERROR]: Failed to read file %s: %s\n", path, strerror(errno));
        return false;
    }
    if (fseek(file, 0, SEEK_END) == -1) {
        fclose(file);
        fprintf(stderr, "[ERROR]: Failed to go to the end of file %s: %s\n", path, strerror(errno));
        return false;
    }
    size_t size = ftell(file);
    if (size == -1) {
        fclose(file);
        fprintf(stderr, "[ERROR]: Failed to get the size of file %s: %s\n", path, strerror(errno));
        return false;
    }
    if (fseek(file, 0, SEEK_SET) == -1) {
        fclose(file);
        fprintf(stderr, "[ERROR]: Failed to go back to the start of file %s: %s\n", path, strerror(errno));
        return false;
    }
    f->content.items = arena_alloc(arena, sizeof(char) * size);
    fread(f->content.items, sizeof(char), size, file);
    f->content.capacity = size;
    f->content.count = size;
#ifdef DEBUG
    fprintf(stderr, "[DEBUG]: Read file %s (size: %zu)\n", path, size);
#endif
    fclose(file);
    return true;
}
