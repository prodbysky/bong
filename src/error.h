#ifndef ERROR_H_
#define ERROR_H_
#include <stddef.h>
#include "fs.h"

typedef struct {
    size_t line;
    size_t col;
} Location;

// Returns a one-indexed location in the source file or the last position in the file
Location get_loc(const SourceFile* file, size_t offset);

// returns:
// \nreturn 1 + 2;
//  ^
ptrdiff_t get_line_begin(const SourceFile* file, size_t offset);

// returns:
// return 1 + 2;\n
//              ^
ptrdiff_t get_line_end(const SourceFile* file, size_t offset);
void bong_error(const SourceFile* source, size_t begin);

#endif
