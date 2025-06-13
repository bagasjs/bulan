#include "codegen.h"
#include <assert.h>
#include "lexer.h"

void generate_fasm_x86_64_win32_program_prolog(Nob_String_Builder *output)
{
    nob_sb_appendf(output, "format MS64 COFF\n");
    nob_sb_appendf(output, "section \".text\" executable\n");
}

void generate_fasm_x86_64_win32_program_epilog(Nob_String_Builder *output)
{
    assert(output);
}

void generate_fasm_x86_64_win32_static_data(Nob_String_Builder *output, Nob_String_Builder static_data)
{
    nob_sb_appendf(output, "section \".data\" executable\n");
    nob_sb_appendf(output, "static_data: db ");
    for(size_t i = 0; i < static_data.count; ++i) {
        if(i > 0) {
            nob_sb_appendf(output, ", ");
        }
        nob_sb_appendf(output, "0x%02X", static_data.items[i]);
    }
    nob_sb_appendf(output, "\n");
}

bool generate_fasm_x86_64_win32_function(Nob_String_Builder *output, Function *fn)
{
    nob_sb_appendf(output, "public %s\n", fn->name);
    nob_sb_appendf(output, "%s:\n", fn->name);
    nob_sb_appendf(output, "    push rbp\n");
    nob_sb_appendf(output, "    mov  rbp, rsp\n");
    nob_sb_appendf(output, "    sub rsp, 16\n");

    for(Block *b = fn->begin; b != NULL; b = b->next) {
        nob_sb_appendf(output, ".block_%zu:\n", b->index);

        for(size_t i = 0; i < b->count; ++i) {
            Inst inst = b->items[i];
            switch(inst.kind) {
                case INST_NOP:
                    break;
                case INST_LOCAL_INIT:
                    if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return false;
                    nob_sb_appendf(output, "    sub rsp, 8\n");
                    nob_sb_appendf(output, "    mov QWORD [rbp - %zu], 0\n", (inst.args[0].local_index + 1) * 8);
                    break;
                case INST_ADD:
                    {

                        if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return false;
                        switch(inst.args[1].kind) {
                            case ARG_LOCAL_INDEX:
                                nob_sb_appendf(output, "    mov rax, QWORD [rbp - %zu]\n", (inst.args[1].local_index + 1) * 8);
                                break;
                            case ARG_INT_VALUE:
                                nob_sb_appendf(output, "    mov rax, %lld\n", inst.args[1].int_value);
                                break;
                            default:
                                compiler_diagf(inst.loc, "Invalid argument 1 for instruction %s with type %s\n", 
                                        display_inst_kind(inst.kind),
                                        display_arg_kind(inst.args[1].kind));
                                break;
                        }
                        switch(inst.args[2].kind) {
                            case ARG_LOCAL_INDEX:
                                nob_sb_appendf(output, "    add rax, QWORD [rbp - %zu]\n", (inst.args[2].local_index + 1) * 8);
                                break;
                            case ARG_INT_VALUE:
                                nob_sb_appendf(output, "    add rax, %lld\n", inst.args[2].int_value);
                                break;
                            default:
                                compiler_diagf(inst.loc, "Invalid argument 2 for instruction %s with type %s\n", 
                                        display_inst_kind(inst.kind),
                                        display_arg_kind(inst.args[2].kind));
                                break;
                        }
                        nob_sb_appendf(output, "    mov QWORD [rbp - %zu], rax\n", (inst.args[0].local_index + 1) * 8);
                    }
                    break;
                case INST_SUB:
                    {

                        if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return false;
                        switch(inst.args[1].kind) {
                            case ARG_LOCAL_INDEX:
                                nob_sb_appendf(output, "    mov rax, QWORD [rbp - %zu]\n", (inst.args[1].local_index + 1) * 8);
                                break;
                            case ARG_INT_VALUE:
                                nob_sb_appendf(output, "    mov rax, %lld\n", inst.args[1].int_value);
                                break;
                            default:
                                compiler_diagf(inst.loc, "Invalid argument 1 for instruction %s with type %s\n", 
                                        display_inst_kind(inst.kind),
                                        display_arg_kind(inst.args[1].kind));
                                break;
                        }
                        switch(inst.args[2].kind) {
                            case ARG_LOCAL_INDEX:
                                nob_sb_appendf(output, "    sub rax, QWORD [rbp - %zu]\n", (inst.args[2].local_index + 1) * 8);
                                break;
                            case ARG_INT_VALUE:
                                nob_sb_appendf(output, "    sub rax, %lld\n", inst.args[2].int_value);
                                break;
                            default:
                                compiler_diagf(inst.loc, "Invalid argument 2 for instruction %s with type %s\n", 
                                        display_inst_kind(inst.kind),
                                        display_arg_kind(inst.args[2].kind));
                                break;
                        }
                        nob_sb_appendf(output, "    mov QWORD [rbp - %zu], rax\n", (inst.args[0].local_index + 1) * 8);
                    }
                    break;
                case INST_LOCAL_ASSIGN:
                    if(!expect_inst_arg(inst, 0, ARG_LOCAL_INDEX)) return false;
                    if(!expect_inst_arg(inst, 1, ARG_INT_VALUE)) return false;
                    switch(inst.args[1].kind) {
                        case ARG_LOCAL_INDEX:
                            nob_sb_appendf(output, "    mov QWORD [rbp - %zu], QWORD [rbp - %zu]\n", 
                                    (inst.args[0].local_index + 1) * 8,
                                    (inst.args[1].local_index + 1) * 8);
                            break;
                        case ARG_INT_VALUE:
                            nob_sb_appendf(output, "    mov QWORD [rbp - %zu], %lld\n", 
                                    (inst.args[0].local_index + 1) * 8, 
                                    inst.args[1].int_value);
                            break;
                        case ARG_STATIC_DATA:
                            nob_sb_appendf(output, "    mov rax, static_data");
                            nob_sb_appendf(output, "    add rax, %zu", inst.args[1].static_offset);
                            nob_sb_appendf(output, "    mov QWORD [rbp - %zu], rax\n", 
                                    (inst.args[0].local_index + 1) * 8);
                            break;
                        default:
                            compiler_diagf(inst.loc, "Invalid argument 1 for instruction %s with type %s\n", 
                                    display_inst_kind(inst.kind),
                                    display_arg_kind(inst.args[1].kind));
                            break;
                    }
                    break;
                case INST_EXTERN:
                    if(!expect_inst_arg(inst, 0, ARG_NAME)) return false;
                    nob_sb_appendf(output, "    extrn %s\n", inst.args[0].name);
                    break;
                case INST_FUNCALL:
                    if(!expect_inst_arg(inst, 0, ARG_NAME)) return false;
                    switch(inst.args[1].kind) {
                        case ARG_LOCAL_INDEX:
                            nob_sb_appendf(output, "    mov  rcx, QWORD[rbp - %zu]\n", (inst.args[1].local_index + 1) * 8);
                            break;
                        case ARG_INT_VALUE:
                            nob_sb_appendf(output, "    mov  rcx, %lld\n", inst.args[1].int_value);
                            break;
                        case ARG_STATIC_DATA:
                            nob_sb_appendf(output, "    mov rcx, static_data\n");
                            nob_sb_appendf(output, "    add rcx, %zu\n", inst.args[1].static_offset);
                            break;
                        case ARG_NONE:
                            break;
                        default:
                            compiler_diagf(inst.loc, "Invalid argument 1 for instruction %s with type %s\n", 
                                    display_inst_kind(inst.kind),
                                    display_arg_kind(inst.args[1].kind));
                            break;
                    }
                    nob_sb_appendf(output, "    call %s\n", inst.args[0].name);
                    break;
            }
        }
    }
    nob_sb_appendf(output, "    mov rsp, rbp\n");
    nob_sb_appendf(output, "    pop rbp\n");
    nob_sb_appendf(output, "    ret\n");
    return true;
}

