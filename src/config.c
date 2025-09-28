#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void help(const char* prog_name) {
    fprintf(stderr, "%s [OPTIONS] <input.bg>\n", prog_name);
    fprintf(stderr, "OPTIONS:\n");
    fprintf(stderr, "  -help: Prints this help message");
}

bool parse_config(int argc, char** argv, Config* out) {
    out->prog_name = *argv++; argc--;
    if (argc == 0) {
        fprintf(stderr, "[ERROR]: No flags/inputs/subcommands provided\n");
        help(out->prog_name);
        exit(0);
    }
    while (argc != 0) {
        if (strcmp(*argv, "-help") == 0) {
            help(out->prog_name);
            exit(0);
        } else {
            if (**argv == '-') {
                fprintf(stderr, "[ERROR]: Not known flag supplied\n");
                help(out->prog_name);
                return false;
            } else if (out->input != NULL) {
                fprintf(stderr, "[ERROR]: Multiple input files provided\n");
                help(out->prog_name);
                return false;
            } else {
                out->input = *argv++; argc--;
            }
        }
    }
    return true;
}
