#include <stdlib.h>
#include <string.h>
#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"


int main(int argc, char** argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    bool rel = false;

    Cmd c = {0};
    nob_log(NOB_INFO, "checking for nasm/ld");
    cmd_append(&c, "which", "nasm");
    bool have_nasm = nob_cmd_run(&c, .stdout_path = "/dev/null");
    cmd_append(&c, "which", "ld");
    bool have_ld = nob_cmd_run(&c, .stdout_path = "/dev/null");

    if (!(have_nasm && have_ld)) {
        nob_log(NOB_ERROR, "Missing nasm and ld, this check should probably be in Shrimp");
        return 1;
    }

    if (argc > 1) rel = strcmp(argv[1], "release") == 0;
    
    mkdir_if_not_exists("build");
    cmd_append(&c, "clang", "src/main.c", "-o", "build/bongc", "-Wall", "-Wextra", "-g");
    if (rel) {
        cmd_append(&c, "-O3");
    } else {
        cmd_append(&c, "-DDEBUG");
    }
    if (!cmd_run_sync_and_reset(&c)) return false;
}
