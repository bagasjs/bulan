#ifndef CODEGEN_H_
#define CODEGEN_H_

#include "lexer.h"
#include "nob.h"
#include "arena.h"
#include <stdint.h>

typedef enum {
    ARG_NONE = 0,
    ARG_INT_VALUE,
    ARG_LOCAL_INDEX,
    ARG_STATIC_DATA,
    ARG_LABEL,
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
        size_t label;
        size_t static_offset;
        char *name;
        int64_t int_value;
        ArgList list;
    };
};

#define MAKE_NONE_ARG()             ((Arg){ .kind = ARG_NONE })
#define MAKE_INT_VALUE_ARG(value)   ((Arg){ .kind = ARG_INT_VALUE,   .int_value     = (value) })
#define MAKE_LOCAL_INDEX_ARG(value) ((Arg){ .kind = ARG_LOCAL_INDEX, .local_index   = (value) })
#define MAKE_LABEL_ARG(value)       ((Arg){ .kind = ARG_LABEL,       .label         = (value) })
#define MAKE_NAME_ARG(value)        ((Arg){ .kind = ARG_NAME,        .name          = (value) })
#define MAKE_LIST_ARG(value)        ((Arg){ .kind = ARG_LIST,        .list          = (value) })
#define MAKE_STATIC_DATA_ARG(offset)((Arg){ .kind = ARG_STATIC_DATA, .static_offset = (offset)})

typedef enum {
    INST_NOP,

    // deprecated?
    INST_EXTERN,

    // deprecated?
    INST_LOCAL_INIT,

    // assign arg[0].local, arg[1]
    INST_LOCAL_ASSIGN,

    // jmp arg[0].block
    INST_JMP,

    // branch arg[0].block, arg[1].block, arg[2]
    INST_BRANCH,

    // funcall arg[0].local, arg[1].name, arg[2].list
    INST_FUNCALL,

    // binop arg[0].local, arg[1], arg[2]
    INST_ADD,
    INST_SUB,
    INST_LT,
    INST_LE,
    INST_GT,
    INST_GE,
    INST_EQ,
    INST_NE,

    // label arg[0].label_index
    INST_LABEL,
} InstKind;

typedef struct {
    Loc loc;
    InstKind kind;
    Arg args[3];
} Inst;

typedef struct {
    Loc loc;
    Inst *items;
    size_t count;
    size_t capacity;
    char *name;
    size_t locals_count;
    size_t labels_count;
} Function;

size_t alloc_local(Function *fn);
size_t alloc_label(Function *fn);
Inst *push_inst(Function *fn, Inst inst);

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

typedef enum {
    VAR_LOCAL  = 0,
    VAR_EXTERN,
} VarStorage;

typedef struct {
    const char *name;
    size_t index;
    VarStorage storage;
} Var;

typedef struct Compiler {
    Arena arena;
    Target target;
    Function func;
    Nob_String_Builder static_data;

    struct {
        Function *items;
        size_t count;
        size_t capacity;
    } funcs;

    struct {
        Var *items;
        size_t count;
        size_t capacity;
    } vars;
} Compiler;


const char *display_target(Target target);

void emit_target_output(Target target, Nob_String_Builder output);

void optimize_program(Compiler *com);
bool generate_x86_64_program(Compiler *com, Nob_String_Builder *output);
bool generate_program(Compiler *com, Nob_String_Builder *output);

#endif // CODEGEN_H_
