#ifndef CODEGEN_H_
#define CODEGEN_H_

#include "lexer.h"
#include "nob.h"
#include "arena.h"

typedef enum {
    ARG_NONE = 0,
    ARG_INT_VALUE,
    ARG_LOCAL_INDEX,
    ARG_NAME,
} ArgKind;

typedef struct {
    ArgKind kind;
    union {
        size_t local_index;
        char *name;
        int64_t int_value;
    };
} Arg;
#define MAKE_NONE_ARG()             ((Arg){ .kind = ARG_NONE }a
#define MAKE_INT_VALUE_ARG(value)   ((Arg){ .kind = ARG_INT_VALUE,   .int_value   = (value) })
#define MAKE_LOCAL_INDEX_ARG(value) ((Arg){ .kind = ARG_LOCAL_INDEX, .local_index = (value) })
#define MAKE_NAME_ARG(value)        ((Arg){ .kind = ARG_NAME,        .name        = (value) })

typedef enum {
    /** Local Variable */
    INST_NOP,
    INST_LOCAL_INIT,
    INST_LOCAL_ASSIGN,

    INST_ADD,
    INST_SUB,
    INST_EXTERN,
    INST_FUNCALL,
} InstKind;

typedef struct {
    Loc loc;
    InstKind kind;
    Arg args[3];
} Inst;

typedef struct Block Block;
struct Block {
    Block *next;
    Inst *items;
    size_t count;
    size_t capacity;
};

typedef struct {
    Arena *arena;
    char *name;
    Block *begin;
    Block *end;
} Function;

void push_block(Function *fn);
void push_inst(Function *fn, Inst inst);
void destroy_function_blocks(Function *fn);

bool expect_inst_arg(Inst inst, int arg_index, ArgKind kind);
const char *display_arg_kind(ArgKind kind);
const char *display_inst_kind(InstKind kind);

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

void generate_fasm_x86_64_win32_program_prolog(Nob_String_Builder *output);
void generate_fasm_x86_64_win32_program_epilog(Nob_String_Builder *output);
bool generate_fasm_x86_64_win32_function(Nob_String_Builder *output, Function *fn);

void generate_html_js_program_prolog(Nob_String_Builder *output);
void generate_html_js_program_epilog(Nob_String_Builder *output);
bool generate_html_js_function(Nob_String_Builder *output, Function *fn);

#endif // CODEGEN_H_
