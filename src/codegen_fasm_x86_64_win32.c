#include "codegen.h"
#include <assert.h>
#include <src/lexer.h>

void generate_fasm_x86_64_win32_program_prolog(Nob_String_Builder *output)
{
    nob_sb_appendf(output, "format MS64 COFF\n");
    nob_sb_appendf(output, "section \".text\" executable\n");
}

void generate_fasm_x86_64_win32_program_epilog(Nob_String_Builder *output)
{
    assert(output);
}

bool generate_fasm_x86_64_win32_function(Nob_String_Builder *output, Function *fn)
{
    nob_sb_appendf(output, "public %s\n", fn->name);
    nob_sb_appendf(output, "%s:\n", fn->name);
    nob_sb_appendf(output, "    push rbp\n");
    nob_sb_appendf(output, "    mov  rbp, rsp\n");
    nob_sb_appendf(output, "    sub rsp, 16\n");

    size_t block_counter = 0;
    for(Block *b = fn->begin; b != NULL; b = b->next) {
        nob_sb_appendf(output, ".block_%zu:\n", block_counter);

        for(size_t i = 0; i < b->count; ++i) {
            Inst inst = b->items[i];
            switch(inst.kind) {
                case INST_LOCAL_INIT:
                    if(!expect_inst_arg_a(inst, ARG_LOCAL_INDEX)) return false;
                    nob_sb_appendf(output, "    sub rsp, 8\n");
                    nob_sb_appendf(output, "    mov QWORD [rbp - %zu], 0\n", (inst.a.local_index + 1) * 8);
                    break;
                case INST_LOCAL_ASSIGN:
                    if(!expect_inst_arg_a(inst, ARG_LOCAL_INDEX)) return false;
                    if(!expect_inst_arg_b(inst, ARG_INT_VALUE)) return false;
                    nob_sb_appendf(output, "    mov QWORD [rbp - %zu], %lld\n", (inst.a.local_index + 1) * 8, inst.b.int_value);
                    break;
                case INST_EXTERN:
                    if(!expect_inst_arg_a(inst, ARG_NAME)) return false;
                    nob_sb_appendf(output, "    extrn %s\n", inst.a.name);
                    break;
                case INST_FUNCALL:
                    if(!expect_inst_arg_a(inst, ARG_NAME)) return false;
                    nob_sb_appendf(output, "    mov  rcx, QWORD[rbp - %zu]\n", (inst.b.local_index + 1) * 8);
                    nob_sb_appendf(output, "    call %s\n", inst.a.name);
                    break;
            }
        }

        block_counter += 1;
    }
    nob_sb_appendf(output, "    mov rsp, rbp\n");
    nob_sb_appendf(output, "    pop rbp\n");
    nob_sb_appendf(output, "    ret\n");
    return true;
}

// bool compile_program(Lexer *lex)
// {
//     Arena arena = {0};
//     Scope scope = {0};
//     Nob_String_Builder output = {0};
// 
//     nob_sb_appendf(&output, "format MS64 COFF\n");
//     nob_sb_appendf(&output, "section \".text\" executable\n");
// 
//     while(lexer_get_token(lex) && lex->token != TOKEN_EOF) {
//         scope.count = 0;
//         if(!lexer_expect_token(lex, TOKEN_FUNCTION)) return -1;
// 
//         if(!lexer_get_and_expect_token(lex, TOKEN_ID)) return -1;
//         nob_sb_appendf(&output, "public %s\n", lex->string);
//         nob_sb_appendf(&output, "%s:\n", lex->string);
//         nob_sb_appendf(&output, "    push rbp\n");
//         nob_sb_appendf(&output, "    mov  rbp, rsp\n");
//         nob_sb_appendf(&output, "    sub rsp, 16\n");
// 
//         if(!lexer_get_and_expect_token(lex, TOKEN_OPAREN)) return -1;
//         if(!lexer_get_and_expect_token(lex, TOKEN_CPAREN)) return -1;
//         if(!lexer_get_and_expect_token(lex, TOKEN_OCURLY)) return -1;
// 
//         while(lexer_get_token(lex) && lex->token != TOKEN_CCURLY) {
//             Loc stmt_loc = lex->loc;
//             switch(lex->token) {
//                 case TOKEN_VAR:
//                     {
//                         lexer_get_and_expect_token(lex, TOKEN_ID);
//                         if(scope_find(scope, lex->string) != NULL) {
//                             compiler_diagf(lex->loc, "Variable with name `%s` is already exists", lex->string);
//                             return false;
//                         }
//                         Var *var = scope_alloc(&scope, arena_strdup(&arena, lex->string));
//                         nob_sb_appendf(&output, "    sub rsp, 8\n");
//                         nob_sb_appendf(&output, "    mov QWORD [rbp - %zu], 0\n", (var->index + 1) * 8);
//                         lexer_get_and_expect_token(lex, TOKEN_SEMICOLON);
//                     } break;
//                 case TOKEN_ID:
//                     {
//                         char *name = arena_strdup(&arena, lex->string);
//                         lexer_get_token(lex);
//                         if(lex->token == TOKEN_EQ) {
//                             // Assignment
//                             Var *var = scope_find(scope, name);
//                             if(var == NULL) {
//                                 compiler_diagf(lex->loc, "Could not find %s in scope", name);
//                                 return false;
//                             }
//                             lexer_get_and_expect_token(lex, TOKEN_INT_LIT);
//                             nob_sb_appendf(&output, "    mov QWORD [rbp - %zu], %llu\n", (var->index + 1) * 8, lex->int_number);
//                         } else if (lex->token == TOKEN_OPAREN) {
//                             // Function call
//                             lexer_get_token(lex);
//                             if(lex->token == TOKEN_ID) {
//                                 Var *arg = scope_find(scope, lex->string);
//                                 if(arg == NULL) {
//                                     compiler_diagf(lex->loc, "Could not find %s in scope", lex->string);
//                                     return false;
//                                 }
//                                 nob_sb_appendf(&output, "    mov  rcx, QWORD [rbp - %zu]\n",  (arg->index + 1) * 8);
//                                 lexer_get_and_expect_token(lex, TOKEN_CPAREN);
//                             } else if (lex->token == TOKEN_CPAREN) {
//                             } else {
//                                 compiler_diagf(lex->loc, "ERROR: expected %s or %s, but got %s", 
//                                         lexer_display_token(TOKEN_ID), lexer_display_token(TOKEN_CPAREN), 
//                                         lexer_display_token(lex->token));
//                             }
//                             nob_sb_appendf(&output, "    call %s\n", name);
//                         } else {
//                             compiler_diagf(stmt_loc, "Invalid follow up token after identifier %s", name);
//                         }
//                         lexer_get_and_expect_token(lex, TOKEN_SEMICOLON);
//                     } break;
//                 case TOKEN_EXTERN:
//                     {
//                         lexer_get_and_expect_token(lex, TOKEN_ID);
//                         nob_sb_appendf(&output, "    extrn %s\n", lex->string);
//                         lexer_get_and_expect_token(lex, TOKEN_SEMICOLON);
//                     } break;
//                 default:
//                     {
//                         compiler_diagf(stmt_loc, "Unexpected token %s to start a statement", lexer_display_token(lex->token));
//                     } break;
//             }
//         }
// 
//         if(!lexer_expect_token(lex, TOKEN_CCURLY)) return -1;
//         nob_sb_appendf(&output, "    mov rsp, rbp\n");
//         nob_sb_appendf(&output, "    pop rbp\n");
//         nob_sb_appendf(&output, "    ret\n");
//     }
// 
//     if(!lexer_get_and_expect_token(lex, TOKEN_EOF)) return -1;
// 
//     if(!nob_write_entire_file("a.S", output.items, output.count)) return -1;
//     nob_da_free(scope);
//     nob_da_free(output);
//     return true;
// }
