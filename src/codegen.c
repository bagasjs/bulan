#include "codegen.h"
#include <assert.h>
#include <src/lexer.h>
#include "nob.h"
#include <stdlib.h>

const char *display_target(Target target)
{
    switch(target) {
        case TARGET_IR: return "ir";
        case TARGET_FASM_X86_64_WIN32: return "fasm_x86-64_win32";
        case TARGET_HTML_JS: return "html-js";
        default:
            assert(0 && "Invalid target in display_target");
    }
    return NULL;
}

void generate_program_prolog(Target target, Nob_String_Builder *output)
{
    switch(target) {
        case TARGET_IR: return;
        case TARGET_FASM_X86_64_WIN32: return generate_fasm_x86_64_win32_program_prolog(output);
        case TARGET_HTML_JS: return generate_html_js_program_prolog(output);
        default:
            assert(0 && "Invalid target in generate_program_prolog");
    }
}

void generate_static_data(Target target, Nob_String_Builder *output, Nob_String_Builder static_data)
{
    switch(target) {
        case TARGET_IR: return;
        case TARGET_FASM_X86_64_WIN32: return generate_fasm_x86_64_win32_static_data(output, static_data);
        default:
            assert(0 && "Invalid target in generate_static_data");
    }
}


void generate_program_epilog(Target target, Nob_String_Builder *output)
{
    switch(target) {
        case TARGET_IR: return;
        case TARGET_FASM_X86_64_WIN32: return generate_fasm_x86_64_win32_program_epilog(output);
        case TARGET_HTML_JS: return generate_html_js_program_epilog(output);
        default:
            assert(0 && "Invalid target in generate_program_epilog");
    }
}

bool generate_function(Target target, Nob_String_Builder *output, Function *fn)
{
    switch(target) {
        case TARGET_IR: dump_function(fn);
        case TARGET_FASM_X86_64_WIN32: return generate_fasm_x86_64_win32_function(output, fn);
        case TARGET_HTML_JS: return generate_html_js_function(output, fn);
        default:
            assert(0 && "Invalid target in generate_program_epilog");
    }
    return false;
}

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
    block->index = fn->blocks_count;
    fn->blocks_count += 1;

    if(fn->begin == NULL) {
        assert(fn->end == NULL);
        fn->begin = block;
        fn->end = block;
    } else {
        fn->end->next = block;
        fn->end = block;
    }
}

Inst *push_inst(Function *fn, Inst inst)
{
    Block *b = fn->end;
    nob_da_append(b, inst);
    return &b->items[b->count - 1];
}

void push_inst_to_block(Block *b, Inst inst)
{
    nob_da_append(b, inst);
}

const char *display_arg_kind(ArgKind kind)
{
    switch(kind) {
        case ARG_NONE: return "none";
        case ARG_INT_VALUE: return "integer value";
        case ARG_LOCAL_INDEX: return "local variable index";
        case ARG_BLOCK_INDEX: return "block index";
        case ARG_NAME: return "name";
        case ARG_STATIC_DATA: return "static data";
        case ARG_LIST: return "list";
        default: assert(0 && "Unreachable: invalid arg kind at display_arg_kind");
    }
    return NULL;
}

const char *display_inst_kind(InstKind kind)
{
    switch(kind) {
        case INST_NOP: return "NOP";
        case INST_LOCAL_INIT: return "LOCAL_INIT";
        case INST_LOCAL_ASSIGN: return "LOCAL_ASSIGN";
        case INST_JMP: return "JMP";
        case INST_JMP_IF: return "JMP_IF";
        case INST_FUNCALL: return "FUNCALL";
        case INST_EXTERN: return "EXTERN";
        case INST_ADD: return "ADD";
        case INST_SUB: return "SUB";
        case INST_LT: return "LT";
        default: assert(0 && "Unreachable: invalid instruction kind at display_inst_kind");
    }
}

bool expect_inst_arg(Inst inst, int arg_index, ArgKind kind)
{
    assert(0 <= arg_index && arg_index <= 3);
    if(inst.args[arg_index].kind != kind) {
        compiler_diagf(inst.loc, "CODEGEN ERROR: Expecting argument '%d' of instruction '%s' to be '%s' but found '%s'",
                arg_index,
                display_inst_kind(inst.kind),
                display_arg_kind(kind),
                display_arg_kind(inst.args[arg_index].kind));
        return false;
    }
    if(kind == ARG_NAME && inst.args[arg_index].name == NULL) {
        compiler_diagf(inst.loc, "CODEGEN ERROR: Generated instruction '%s' argument '%d' is a name with value null", arg_index,
                display_inst_kind(inst.kind));
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
                    if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return;
                    printf("    LOCAL_INIT(LOCAL(%zu))\n", inst.args[0].local_index);
                    break;
                case INST_LOCAL_ASSIGN:
                    if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return;
                    printf("    LOCAL_ASSIGN(LOCAL(%zu), ", inst.args[0].local_index);
                    switch(inst.args[1].kind) {
                        case ARG_LOCAL_INDEX:
                            printf("LOCAL(%zu))\n", inst.args[1].local_index);
                            break;
                        case ARG_INT_VALUE:
                            printf("VALUE(%lld))\n", inst.args[1].int_value);
                            break;
                        case ARG_STATIC_DATA:
                            printf("STATIC(%zu))\n", inst.args[1].static_offset);
                            break;
                        default:
                            compiler_diagf(inst.loc, "CODEGEN ERROR: Invalid argument 1 for instruction %s with type %s", 
                                    display_inst_kind(inst.kind),
                                    display_arg_kind(inst.args[1].kind));
                            break;
                    }
                    break;
                case INST_EXTERN:
                    if(!expect_inst_arg(inst, 0, ARG_NAME)) return;
                    printf("    EXTERN(%s)\n", inst.args[0].name);
                    break;
                case INST_FUNCALL:
                    if(!expect_inst_arg(inst, 0, ARG_NAME)) return;
                    printf("    FUNCALL(%s)\n", inst.args[0].name);
                    break;
                case INST_JMP_IF:
                    if(!expect_inst_arg(inst, 0, ARG_BLOCK_INDEX)) return;
                    printf("    JMP_IF(BLOCK(%zu), ", inst.args[0].block_index);
                    switch(inst.args[1].kind) {
                        case ARG_LOCAL_INDEX:
                            printf("LOCAL(%zu))\n", inst.args[1].local_index);
                            break;
                        case ARG_INT_VALUE:
                            printf("VALUE(%lld))\n", inst.args[1].int_value);
                            break;
                        case ARG_STATIC_DATA:
                            printf("STATIC(%zu)\n", inst.args[1].static_offset);
                            break;
                        default:
                            compiler_diagf(inst.loc, "CODEGEN ERROR: Invalid argument 1 for instruction %s with type %s", 
                                    display_inst_kind(inst.kind),
                                    display_arg_kind(inst.args[1].kind));
                            break;
                    }
                    break;
                case INST_LT:
                    if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return;
                    printf("    LT(LOCAL(%zu), ", inst.args[0].local_index);
                    switch(inst.args[1].kind) {
                        case ARG_LOCAL_INDEX:
                            printf("LOCAL(%zu), ", inst.args[1].local_index);
                            break;
                        case ARG_INT_VALUE:
                            printf("VALUE(%lld), ", inst.args[1].int_value);
                            break;
                        case ARG_STATIC_DATA:
                            printf("STATIC(%zu), ", inst.args[1].static_offset);
                            break;
                        default:
                            compiler_diagf(inst.loc, "CODEGEN ERROR: Invalid argument 1 for instruction %s with type %s", 
                                    display_inst_kind(inst.kind),
                                    display_arg_kind(inst.args[1].kind));
                            break;
                    }
                    switch(inst.args[2].kind) {
                        case ARG_LOCAL_INDEX:
                            printf("LOCAL(%zu))\n", inst.args[2].local_index);
                            break;
                        case ARG_INT_VALUE:
                            printf("VALUE(%lld))\n", inst.args[2].int_value);
                            break;
                        case ARG_STATIC_DATA:
                            printf("STATIC(%zu)\n", inst.args[2].static_offset);
                            break;
                        default:
                            compiler_diagf(inst.loc, "CODEGEN ERROR: Invalid argument 2 for instruction %s with type %s", 
                                    display_inst_kind(inst.kind),
                                    display_arg_kind(inst.args[2].kind));
                            break;
                    }
                    break;
                case INST_SUB:
                    if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return;
                    printf("    SUB(LOCAL(%zu), ", inst.args[0].local_index);
                    switch(inst.args[1].kind) {
                        case ARG_LOCAL_INDEX:
                            printf("LOCAL(%zu), ", inst.args[1].local_index);
                            break;
                        case ARG_INT_VALUE:
                            printf("VALUE(%lld), ", inst.args[1].int_value);
                            break;
                        case ARG_STATIC_DATA:
                            printf("STATIC(%zu), ", inst.args[1].static_offset);
                            break;
                        default:
                            compiler_diagf(inst.loc, "CODEGEN ERROR: Invalid argument 1 for instruction %s with type %s", 
                                    display_inst_kind(inst.kind),
                                    display_arg_kind(inst.args[1].kind));
                            break;
                    }
                    switch(inst.args[2].kind) {
                        case ARG_LOCAL_INDEX:
                            printf("LOCAL(%zu))\n", inst.args[2].local_index);
                            break;
                        case ARG_INT_VALUE:
                            printf("VALUE(%lld))\n", inst.args[2].int_value);
                            break;
                        case ARG_STATIC_DATA:
                            printf("STATIC(%zu)\n", inst.args[2].static_offset);
                            break;
                        default:
                            compiler_diagf(inst.loc, "CODEGEN ERROR: Invalid argument 2 for instruction %s with type %s", 
                                    display_inst_kind(inst.kind),
                                    display_arg_kind(inst.args[2].kind));
                            break;
                    }
                    break;
                case INST_ADD:
                    if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return;
                    printf("    ADD(LOCAL(%zu), ", inst.args[0].local_index);
                    switch(inst.args[1].kind) {
                        case ARG_LOCAL_INDEX:
                            printf("LOCAL(%zu), ", inst.args[1].local_index);
                            break;
                        case ARG_INT_VALUE:
                            printf("VALUE(%lld), ", inst.args[1].int_value);
                            break;
                        case ARG_STATIC_DATA:
                            printf("STATIC(%zu), ", inst.args[1].static_offset);
                            break;
                        default:
                            compiler_diagf(inst.loc, "CODEGEN ERROR: Invalid argument 1 for instruction %s with type %s", 
                                    display_inst_kind(inst.kind),
                                    display_arg_kind(inst.args[1].kind));
                            break;
                    }
                    switch(inst.args[2].kind) {
                        case ARG_LOCAL_INDEX:
                            printf("LOCAL(%zu))\n", inst.args[2].local_index);
                            break;
                        case ARG_INT_VALUE:
                            printf("VALUE(%lld))\n", inst.args[2].int_value);
                            break;
                        case ARG_STATIC_DATA:
                            printf("STATIC(%zu)\n", inst.args[2].static_offset);
                            break;
                        default:
                            compiler_diagf(inst.loc, "CODEGEN ERROR: Invalid argument 2 for instruction %s with type %s", 
                                    display_inst_kind(inst.kind),
                                    display_arg_kind(inst.args[2].kind));
                            break;
                    }
                    break;
                case INST_JMP:
                    if(!expect_inst_arg(inst, 0, ARG_BLOCK_INDEX)) return;
                    printf("    JMP(BLOCK(%zu))\n", inst.args[0].block_index);
                    break;
                default:
                    assert(0 && "Invalid instruction kind");
            }
        }
        block_counter += 1;
    }
}
