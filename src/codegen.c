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
        // case TARGET_HTML_JS: return "html-js";
        default:
            assert(0 && "Invalid target in display_target");
    }
    return NULL;
}

bool generate_program(Program *prog, Nob_String_Builder *output)
{
    switch(prog->target) {
        case TARGET_IR: 
            {
                for(size_t i = 0; i < prog->count_funcs; ++i) {
                    dump_function(&prog->funcs[i]);
                }
            } break;
        case TARGET_FASM_X86_64_WIN32:
            return generate_x86_64_program(prog, output);
        default:
            assert(0 && "Invalid target in generate_program");
    }
    return true;
}

void optimize_function(Program *prog, Function *fn)
{
}

void optimize_program(Program *prog)
{
}

Inst *push_inst(Function *fn, Inst inst)
{
    nob_da_append(fn, inst);
    return &fn->items[fn->count - 1];
}

size_t alloc_local(Function *fn)
{
    size_t local = fn->locals_count;
    fn->locals_count += 1;
    return local;
}

size_t alloc_label(Function *fn)
{
    size_t label = fn->labels_count;
    fn->labels_count += 1;
    return label;
}

const char *display_arg_kind(ArgKind kind)
{
    switch(kind) {
        case ARG_NONE: return "none";
        case ARG_INT_VALUE: return "integer value";
        case ARG_LOCAL_INDEX: return "local variable index";
        case ARG_LABEL: return "label";
        case ARG_NAME: return "name";
        case ARG_STATIC_DATA: return "static data";
        case ARG_LIST: return "list";
        case ARG_DEREF: return "deref";
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
        case INST_FUNCALL: return "FUNCALL";
        case INST_EXTERN: return "EXTERN";
        case INST_ADD: return "ADD";
        case INST_SUB: return "SUB";
        case INST_MUL: return "MUL";
        case INST_LT: return "LT";
        case INST_LE: return "LE";
        case INST_GT: return "GT";
        case INST_GE: return "GE";
        case INST_EQ: return "EQ";
        case INST_NE: return "NE";
        case INST_BRANCH: return "BRANCH";
        case INST_LABEL: return "LABEL";
        case INST_STORE: return "STORE";
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

void dump_arg(Arg arg, const char *end)
{
    switch(arg.kind) {
        case ARG_NONE:
            printf("NONE%s", end);
            break;
        case ARG_LABEL:
            printf(".L%zu%s", arg.local_index, end);
            break;
        case ARG_LOCAL_INDEX:
            printf("#%zu%s", arg.local_index, end);
            break;
        case ARG_STATIC_DATA:
            printf("static[%zu]%s", arg.static_offset, end);
            break;
        case ARG_NAME:
            printf("\"%s\"%s", arg.name, end);
            break;
        case ARG_INT_VALUE:
            printf("$%lld%s", arg.int_value, end);
            break;
        case ARG_DEREF:
            printf("[#%zu]%s", arg.local_index, end);
            break;
        case ARG_LIST:
            printf("(");
            for(size_t i = 0; i < arg.list.count; ++i) {
                if(i > 0) printf(", ");
                dump_arg(arg.list.items[i], "");
            }
            printf(")%s",end);
            break;
    }
}

void dump_function(Function *fn)
{
    printf("%s() [locals=%zu]\n", fn->name, fn->locals_count);
    for(size_t i = 0; i < fn->count; ++i) {
        Inst inst = fn->items[i];
        switch(inst.kind) {
            case INST_LABEL:
                printf(".L%zu:\n", inst.args[0].label);
                break;
            case INST_LOCAL_INIT:
                if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return;
                printf("    local_init   #%zu\n", inst.args[0].local_index);
                break;
            case INST_LOCAL_ASSIGN:
                if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return;
                printf("    ");
                dump_arg(inst.args[0], " = ");
                dump_arg(inst.args[1], "\n");
                break;
            case INST_STORE:
                if(!expect_inst_arg(inst, 0, ARG_DEREF)) return;
                printf("    _  = store");
                dump_arg(inst.args[0], ",");
                dump_arg(inst.args[1], "\n");
                break;
            case INST_EXTERN:
                if(!expect_inst_arg(inst, 0, ARG_NAME)) return;
                printf("    _ = extern ");
                dump_arg(inst.args[0], "\n");
                break;
            case INST_FUNCALL:
                {
                    if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return;
                    if(!expect_inst_arg(inst, 1, ARG_NAME)) return;
                    if(!expect_inst_arg(inst, 2, ARG_LIST)) return;
                    printf("    funcall ");
                    dump_arg(inst.args[0], " = ");
                    dump_arg(inst.args[1], ", ");
                    dump_arg(inst.args[2], "\n");
                }
                break;
            case INST_ADD:
                if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return;
                printf("    #%zu = add ", inst.args[0].local_index);
                dump_arg(inst.args[1], ", ");
                dump_arg(inst.args[2], "\n");
                break;
            case INST_MUL:
                if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return;
                printf("    #%zu = mul ", inst.args[0].local_index);
                dump_arg(inst.args[1], ", ");
                dump_arg(inst.args[2], "\n");
                break;
            case INST_SUB:
                if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return;
                printf("    #%zu = sub ", inst.args[0].local_index);
                // TODO: expect args 1 to either ARG_LOCAL_INDEX, ARG_INT_VALUE or ARG_STATIC_DATA
                // TODO: expect args 2 to either ARG_LOCAL_INDEX, ARG_INT_VALUE or ARG_STATIC_DATA
                dump_arg(inst.args[1], ", ");
                dump_arg(inst.args[2], "\n");
                break;
            case INST_LT:
                if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return;
                printf("    #%zu = lt ", inst.args[0].local_index);
                // TODO: expect args 1 to either ARG_LOCAL_INDEX, ARG_INT_VALUE or ARG_STATIC_DATA
                // TODO: expect args 2 to either ARG_LOCAL_INDEX, ARG_INT_VALUE or ARG_STATIC_DATA
                dump_arg(inst.args[1], ", ");
                dump_arg(inst.args[2], "\n");
                break;
            case INST_LE:
                if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return;
                printf("    #%zu = le ", inst.args[0].local_index);
                // TODO: expect args 1 to either ARG_LOCAL_INDEX, ARG_INT_VALUE or ARG_STATIC_DATA
                // TODO: expect args 2 to either ARG_LOCAL_INDEX, ARG_INT_VALUE or ARG_STATIC_DATA
                dump_arg(inst.args[1], ", ");
                dump_arg(inst.args[2], "\n");
                break;
            case INST_GT:
                if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return;
                printf("    #%zu = gt ", inst.args[0].local_index);
                // TODO: expect args 1 to either ARG_LOCAL_INDEX, ARG_INT_VALUE or ARG_STATIC_DATA
                // TODO: expect args 2 to either ARG_LOCAL_INDEX, ARG_INT_VALUE or ARG_STATIC_DATA
                dump_arg(inst.args[1], ", ");
                dump_arg(inst.args[2], "\n");
                break;
            case INST_GE:
                if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return;
                printf("    #%zu = ge ", inst.args[0].local_index);
                // TODO: expect args 1 to either ARG_LOCAL_INDEX, ARG_INT_VALUE or ARG_STATIC_DATA
                // TODO: expect args 2 to either ARG_LOCAL_INDEX, ARG_INT_VALUE or ARG_STATIC_DATA
                dump_arg(inst.args[1], ", ");
                dump_arg(inst.args[2], "\n");
                break;
            case INST_EQ:
                if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return;
                printf("    #%zu = eq ", inst.args[0].local_index);
                // TODO: expect args 1 to either ARG_LOCAL_INDEX, ARG_INT_VALUE or ARG_STATIC_DATA
                // TODO: expect args 2 to either ARG_LOCAL_INDEX, ARG_INT_VALUE or ARG_STATIC_DATA
                dump_arg(inst.args[1], ", ");
                dump_arg(inst.args[2], "\n");
                break;
            case INST_NE:
                if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return;
                printf("    #%zu = ne ", inst.args[0].local_index);
                // TODO: expect args 1 to either ARG_LOCAL_INDEX, ARG_INT_VALUE or ARG_STATIC_DATA
                // TODO: expect args 2 to either ARG_LOCAL_INDEX, ARG_INT_VALUE or ARG_STATIC_DATA
                dump_arg(inst.args[1], ", ");
                dump_arg(inst.args[2], "\n");
                break;
            case INST_BRANCH:
                if(!expect_inst_arg(inst, 0, ARG_LABEL)) return;
                if(!expect_inst_arg(inst, 1, ARG_LABEL)) return;
                printf("    _ = branch ");
                dump_arg(inst.args[0], ", ");
                dump_arg(inst.args[1], ", ");
                dump_arg(inst.args[2], "\n");
                break;
            case INST_JMP:
                if(!expect_inst_arg(inst, 0, ARG_LABEL)) return;
                printf("    _ = jmp .L%zu\n", inst.args[0].label);
                break;
            default:
                assert(0 && "Invalid instruction kind");
        }
    }
}
