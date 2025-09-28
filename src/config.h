#ifndef CONFIG_H_
#define CONFIG_H_

#include <stdbool.h>

typedef struct {
    const char* prog_name;
    const char* input;
} Config;

bool parse_config(int argc, char** argv, Config* out);
#endif
