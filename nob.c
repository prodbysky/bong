#include <string.h>
#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    bool rel = false;
    if (argc > 1) {
        rel = strcmp(argv[1], "release") == 0;
    }
    Cmd c = {0};
    mkdir_if_not_exists("build");
    cmd_append(&c, "clang", "src/main.c", "-o", "build/bongc", "-Wall", "-Wextra", "-g");
    if (rel) {
        cmd_append(&c, "-O3");
    } else {
        cmd_append(&c, "-DDEBUG");
    }
    if (!cmd_run_sync_and_reset(&c)) return false;
}
