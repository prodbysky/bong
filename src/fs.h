#ifndef FS_H_
#define FS_H_

#include "str.h"
#include "arena.h"
#include <stdbool.h>
typedef struct {
    String content;
    const char* name;
} SourceFile;

bool read_entire_file(const char* path, SourceFile* f, Arena* arena);
#endif
