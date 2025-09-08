#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Cmd c = {0};
    mkdir_if_not_exists("build");
    cmd_append(&c, "cc", "src/main.c", "-o", "build/bongc", "-Wall", "-Wextra", "-g");
    if (!cmd_run_sync_and_reset(&c)) return false;
}
