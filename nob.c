#define NOB_IMPLEMENTATION
#include "./thirdparty/nob.h"

#define BUILD_DIR "build"

bool build_obj(Nob_Cmd *cmd, const char *output, const char *input)
{
    if(nob_needs_rebuild1(output, input)) {
        nob_cmd_append(cmd, "clang");
        nob_cmd_append(cmd, "-Wall", "-Wextra", "-D_CRT_SECURE_NO_WARNINGS", "-I", "thirdparty", "-g", "-fsanitize=address");
        nob_cmd_append(cmd, "-c", "-o", output);
        nob_cmd_append(cmd, input);
        return nob_cmd_run_sync_and_reset(cmd);
    }
    return true;
}

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    nob_mkdir_if_not_exists(BUILD_DIR);

    Nob_Cmd cmd = {0};
    if(!build_obj(&cmd, BUILD_DIR"/nob.o", "./thirdparty/nob.c")) return -1;
    if(!build_obj(&cmd, BUILD_DIR"/arena.o", "./thirdparty/arena.c")) return -1;
    if(!build_obj(&cmd, BUILD_DIR"/flag.o", "./thirdparty/flag.c")) return -1;

    nob_cmd_append(&cmd, "clang");
    nob_cmd_append(&cmd, "-Wall", "-Wextra", "-g", "-fsanitize=address");
    nob_cmd_append(&cmd, "-D_CRT_SECURE_NO_WARNINGS");
    nob_cmd_append(&cmd, "-I", "thirdparty");
    nob_cmd_append(&cmd, "-I", "src");
    nob_cmd_append(&cmd, "-o", BUILD_DIR"/blnc.exe");
    nob_cmd_append(&cmd, "./src/bulan.c");
    nob_cmd_append(&cmd, "./src/lexer.c");
    nob_cmd_append(&cmd, "./src/codegen.c");
    nob_cmd_append(&cmd, "./src/codegen_fasm_x86_64_win32.c");
    nob_cmd_append(&cmd, "./build/nob.o");
    nob_cmd_append(&cmd, "./build/arena.o");
    nob_cmd_append(&cmd, "./build/flag.o");
    nob_cmd_run_sync_and_reset(&cmd);

    return 0;
}
