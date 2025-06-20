#ifndef CODEGEN_H_
#define CODEGEN_H_

#include "lexer.h"
#include "nob.h"
#include "arena.h"

typedef enum {
    ARG_NONE = 0,
    ARG_INT_VALUE,
    ARG_LOCAL_INDEX,
    ARG_STATIC_DATA,
    ARG_BLOCK_INDEX,
    ARG_LIST,
    ARG_NAME,
} ArgKind;

typedef struct Arg Arg;
typedef struct {
    Arg *items;
    size_t count;
    size_t capacity;
} ArgList;
struct Arg {
    ArgKind kind;
    union {
        size_t local_index;
        size_t block_index;
        size_t static_offset;
        char *name;
        int64_t int_value;
        ArgList list;
    };
};

#define MAKE_NONE_ARG()             ((Arg){ .kind = ARG_NONE })
#define MAKE_INT_VALUE_ARG(value)   ((Arg){ .kind = ARG_INT_VALUE,   .int_value     = (value) })
#define MAKE_LOCAL_INDEX_ARG(value) ((Arg){ .kind = ARG_LOCAL_INDEX, .local_index   = (value) })
#define MAKE_BLOCK_INDEX_ARG(value) ((Arg){ .kind = ARG_BLOCK_INDEX, .block_index   = (value) })
#define MAKE_NAME_ARG(value)        ((Arg){ .kind = ARG_NAME,        .name          = (value) })
#define MAKE_LIST_ARG(value)        ((Arg){ .kind = ARG_LIST,        .list          = (value) })
#define MAKE_STATIC_DATA_ARG(offset)((Arg){ .kind = ARG_STATIC_DATA, .static_offset = (offset)})

typedef enum {
    /** Local Variable */
    INST_NOP,
    INST_LOCAL_INIT,
    INST_LOCAL_ASSIGN,
    INST_JMP,
    INST_JMP_IF,
    INST_BRANCH,
    INST_FUNCALL,
    INST_EXTERN,

    INST_ADD,
    INST_SUB,
    INST_LT,
} InstKind;

typedef struct {
    Loc loc;
    InstKind kind;
    Arg args[3];
} Inst;

typedef struct Block Block;
struct Block {
    Block *next;
    size_t index;

    Inst *items;
    size_t count;
    size_t capacity;
};

typedef struct {
    Arena *arena;
    char *name;
    size_t blocks_count;
    Block *begin;
    Block *end;
} Function;

void push_block(Function *fn);
Inst *push_inst(Function *fn, Inst inst);
void push_inst_to_block(Block *b, Inst inst);
void destroy_function_blocks(Function *fn);

bool expect_inst_arg(Inst inst, int arg_index, ArgKind kind);
const char *display_arg_kind(ArgKind kind);
const char *display_inst_kind(InstKind kind);

void dump_arg(Arg arg, const char *end);
void dump_function(Function *fn);


// Target's Generator

typedef enum {
    _INVALID_TARGET = 0,
    TARGET_IR,
    TARGET_FASM_X86_64_WIN32,
    TARGET_HTML_JS,

    _COUNT_TARGETS,
} Target;

const char *display_target(Target target);

void generate_program_prolog(Target target, Nob_String_Builder *output);
void generate_program_epilog(Target target, Nob_String_Builder *output);
bool generate_function(Target target, Nob_String_Builder *output, Function *fn);
void generate_static_data(Target target, Nob_String_Builder *output, Nob_String_Builder static_data);

void generate_fasm_x86_64_win32_program_prolog(Nob_String_Builder *output);
void generate_fasm_x86_64_win32_program_epilog(Nob_String_Builder *output);
void generate_fasm_x86_64_win32_static_data(Nob_String_Builder *output, Nob_String_Builder static_data);
bool generate_fasm_x86_64_win32_function(Nob_String_Builder *output, Function *fn);

void generate_nasm_x86_64_win32_program_prolog(Nob_String_Builder *output);
void generate_nasm_x86_64_win32_program_epilog(Nob_String_Builder *output);
void generate_nasm_x86_64_win32_static_data(Nob_String_Builder *output, Nob_String_Builder static_data);
bool generate_nasm_x86_64_win32_function(Nob_String_Builder *output, Function *fn);

void generate_html_js_program_prolog(Nob_String_Builder *output);
void generate_html_js_program_epilog(Nob_String_Builder *output);
bool generate_html_js_function(Nob_String_Builder *output, Function *fn);

#endif // CODEGEN_H_
