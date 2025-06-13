#include "codegen.h"
#include <assert.h>
#include <src/lexer.h>
#include "nob.h"
#include <stdlib.h>

void destroy_function_blocks(Function *fn)
{
    Block *b = fn->begin;
    while(b != NULL) {
        Block *b0 = b;
        b = b->next;
        nob_da_free(*b0);
    }
}

void push_block(Function *fn)
{
    Block *block = arena_alloc(&fn->arena, sizeof(*block));
    memset(block, 0, sizeof(*block));
    if(fn->begin == NULL) {
        assert(fn->end == NULL);
        fn->begin = block;
        fn->end = block;
    } else {
        fn->end->next = block;
        fn->end = block;
    }
}

void push_inst(Function *fn, Inst inst)
{
    Block *b = fn->end;
    nob_da_append(b, inst);
}


const char *display_arg_kind(ArgKind kind)
{
    switch(kind) {
        case ARG_NONE: return "none";
        case ARG_INT_VALUE: return "integer value";
        case ARG_LOCAL_INDEX: return "local variable index";
        case ARG_NAME: return "name";
        default: assert(0 && "Unreachable: invalid arg kind at display_arg_kind");
    }
}

const char *display_inst_kind(InstKind kind)
{
    switch(kind) {
        case INST_INIT_LOCAL: return "INIT_LOCAL";
        case INST_ASSIGN_LOCAL: return "ASSIGN_LOCAL";
        case INST_EXTERN: return "EXTERN";
        case INST_FUNCALL: return "FUNCALL";
        default: assert(0 && "Unreachable: invalid instruction kind at display_inst_kind");
    }
}

void expect_inst_arg_a(Inst inst, ArgKind kind)
{
    if(inst.a.kind != kind) {
        compiler_diagf(inst.loc, "CODEGEN ERROR: Expecting argument 'a' of instruction '%s' to be '%s' but found '%s'",
                display_inst_kind(inst.kind),
                display_arg_kind(kind),
                display_arg_kind(inst.a.kind));
        exit(-1);
    }
    if(kind == ARG_NAME && inst.a.name == NULL) {
        compiler_diagf(inst.loc, "Generated instruction '%s' argument a is a name with value null", display_inst_kind(inst.kind));
        exit(-1);
    }
}

void expect_inst_arg_b(Inst inst, ArgKind kind)
{
    if(inst.b.kind != kind) {
        compiler_diagf(inst.loc, "CODEGEN ERROR: Expecting argument 'b' of instruction '%s' to be '%s' but found '%s'",
                display_inst_kind(inst.kind),
                display_arg_kind(kind),
                display_arg_kind(inst.b.kind));
        exit(-1);
    }
    if(kind == ARG_NAME && inst.b.name == NULL) {
        compiler_diagf(inst.loc, "Generated instruction '%s' argument b is a name with value null", display_inst_kind(inst.kind));
        exit(-1);
    }
}

