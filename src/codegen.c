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
    Block *block = arena_alloc(fn->arena, sizeof(*block));
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
        case INST_LOCAL_INIT: return "LOCAL_INIT";
        case INST_LOCAL_ASSIGN: return "LOCAL_ASSIGN";
        case INST_EXTERN: return "EXTERN";
        case INST_FUNCALL: return "FUNCALL";
        default: assert(0 && "Unreachable: invalid instruction kind at display_inst_kind");
    }
}

bool expect_inst_arg_a(Inst inst, ArgKind kind)
{
    if(inst.a.kind != kind) {
        compiler_diagf(inst.loc, "CODEGEN ERROR: Expecting argument 'a' of instruction '%s' to be '%s' but found '%s'",
                display_inst_kind(inst.kind),
                display_arg_kind(kind),
                display_arg_kind(inst.a.kind));
        return false;
    }
    if(kind == ARG_NAME && inst.a.name == NULL) {
        compiler_diagf(inst.loc, "Generated instruction '%s' argument a is a name with value null", display_inst_kind(inst.kind));
        return false;
    }
    return true;
}

bool expect_inst_arg_b(Inst inst, ArgKind kind)
{
    if(inst.b.kind != kind) {
        compiler_diagf(inst.loc, "CODEGEN ERROR: Expecting argument 'b' of instruction '%s' to be '%s' but found '%s'",
                display_inst_kind(inst.kind),
                display_arg_kind(kind),
                display_arg_kind(inst.b.kind));
        return false;
    }
    if(kind == ARG_NAME && inst.b.name == NULL) {
        compiler_diagf(inst.loc, "Generated instruction '%s' argument b is a name with value null", display_inst_kind(inst.kind));
        return false;
    }
    return true;
}

// DUMP

#include <stdio.h>
void dump_function(Function *fn)
{
    printf("%s()\n", fn->name);
    size_t block_counter = 0;
    for(Block *b = fn->begin; b != NULL; b = b->next) {
        printf("block_%zu:\n", block_counter);
        for(size_t i = 0; i < b->count; ++i) {
            Inst inst = b->items[i];
            switch(inst.kind) {
                case INST_LOCAL_INIT:
                    if(!expect_inst_arg_a(inst, ARG_LOCAL_INDEX)) return;
                    printf("    LOCAL_INIT(%zu)\n", inst.a.local_index);
                    break;
                case INST_LOCAL_ASSIGN:
                    if(!expect_inst_arg_a(inst, ARG_LOCAL_INDEX)) return;
                    if(!expect_inst_arg_b(inst, ARG_INT_VALUE)) return;
                    printf("    LOCAL_ASSIGN(%zu, %lld)\n", inst.a.local_index, inst.b.int_value);
                    break;
                case INST_EXTERN:
                    if(!expect_inst_arg_a(inst, ARG_NAME)) return;
                    printf("    EXTERN(%s)\n", inst.a.name);
                    break;
                case INST_FUNCALL:
                    if(!expect_inst_arg_a(inst, ARG_NAME)) return;
                    printf("    FUNCALL(%s)\n", inst.a.name);
                    break;
                default:
                    assert(0 && "Invalid instruction kind");
            }
        }
        block_counter += 1;
    }
}
