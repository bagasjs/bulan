#define NOB_IMPLEMENTATION
#include "./thirdparty/nob.h"

#define BUILD_DIR "build"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    nob_mkdir_if_not_exists(BUILD_DIR);

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "clang");
    nob_cmd_append(&cmd, "-Wall", "-Wextra", "-D_CRT_SECURE_NO_WARNINGS", "-I", "thirdparty", "-g", "-fsanitize=address");
    nob_cmd_append(&cmd, "-o", BUILD_DIR"/blnc.exe");
    nob_cmd_append(&cmd, "./src/main.c");
    nob_cmd_append(&cmd, "./src/lexer.c");
    nob_cmd_append(&cmd, "./src/codegen.c");
    nob_cmd_append(&cmd, "./src/codegen_fasm_x86_64_win32.c");
    nob_cmd_append(&cmd, "./thirdparty/nob.c");
    nob_cmd_append(&cmd, "./thirdparty/arena.c");
    nob_cmd_run_sync_and_reset(&cmd);

    return 0;
}
