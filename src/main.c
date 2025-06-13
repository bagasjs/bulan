#include <stdio.h>
#include <string.h>
#include "nob.h"
#include "arena.h"
#include "lexer.h"
#include "codegen.h"

typedef struct {
    const char *name;
    size_t index;
} Var;

typedef struct {
    Var *items;
    size_t count;
    size_t capacity;
} Scope;

Var *scope_find(const Scope *scope, const char *name)
{
    for(size_t i = 0; i < scope->count; ++i) {
        if(strcmp(scope->items[i].name, name) == 0) {
            return &scope->items[i];
        }
    }
    return NULL;
}

Var *scope_alloc(Scope *scope, const char *name)
{
    Var b = (Var){ .name = name, .index = scope->count };
    nob_da_append(scope, b);
    return &scope->items[scope->count - 1];
}

bool compile_function(Function *fn, Lexer *lex, Scope *scope, Nob_String_Builder *output)
{
    Loc top_loc = lex->loc;
    scope->count = 0;
    if(!lexer_expect_token(lex, TOKEN_FUNCTION)) return false;
    if(!lexer_get_and_expect_token(lex, TOKEN_ID)) return false;
    fn->name  = arena_strdup(fn->arena, lex->string);

    if(!lexer_get_and_expect_token(lex, TOKEN_OPAREN)) return false;
    if(!lexer_get_and_expect_token(lex, TOKEN_CPAREN)) return false;
    if(!lexer_get_and_expect_token(lex, TOKEN_OCURLY)) return false;
    push_block(fn);

    while(lexer_get_token(lex) && lex->token != TOKEN_CCURLY) {
        Loc stmt_loc = lex->loc;
        switch(lex->token) {
            case TOKEN_VAR:
                {
                    lexer_get_and_expect_token(lex, TOKEN_ID);
                    if(scope_find(scope, lex->string) != NULL) {
                        compiler_diagf(lex->loc, "Variable with name `%s` is already exists", lex->string);
                        return false;
                    }
                    Var *var = scope_alloc(scope, arena_strdup(fn->arena, lex->string));
                    push_inst(fn, (Inst) {
                        .loc = stmt_loc,
                        .kind = INST_LOCAL_INIT,
                        .a = MAKE_LOCAL_INDEX_ARG(var->index),
                    });
                    lexer_get_and_expect_token(lex, TOKEN_SEMICOLON);
                } break;
            case TOKEN_ID:
                {
                    char *name = arena_strdup(fn->arena, lex->string);
                    lexer_get_token(lex);
                    if(lex->token == TOKEN_EQ) {
                        // Assignment
                        Var *var = scope_find(scope, name);
                        if(var == NULL) {
                            compiler_diagf(lex->loc, "Could not find %s in scope", name);
                            return false;
                        }
                        lexer_get_and_expect_token(lex, TOKEN_INT_LIT);
                        push_inst(fn, (Inst) {
                            .loc = stmt_loc,
                            .kind = INST_LOCAL_ASSIGN,
                            .a = MAKE_LOCAL_INDEX_ARG(var->index),
                            .b = MAKE_INT_VALUE_ARG(lex->int_number),
                        });
                    } else if (lex->token == TOKEN_OPAREN) {
                        // Function call
                        lexer_get_token(lex);
                        if(lex->token == TOKEN_ID) {
                            Var *arg = scope_find(scope, lex->string);
                            if(arg == NULL) {
                                compiler_diagf(lex->loc, "Could not find %s in scope", lex->string);
                                return false;
                            }
                            push_inst(fn, (Inst) {
                                .loc = stmt_loc,
                                .kind = INST_FUNCALL,
                                .a = MAKE_NAME_ARG(name),
                                .b = MAKE_LOCAL_INDEX_ARG(arg->index)
                            });
                            lexer_get_and_expect_token(lex, TOKEN_CPAREN);
                        } else if (lex->token == TOKEN_CPAREN) {
                            push_inst(fn, (Inst) {
                                .loc = stmt_loc,
                                .kind = INST_FUNCALL,
                                .a = MAKE_NAME_ARG(name),
                            });
                        } else {
                            compiler_diagf(lex->loc, "ERROR: expected %s or %s, but got %s", 
                                    lexer_display_token(TOKEN_ID), lexer_display_token(TOKEN_CPAREN), 
                                    lexer_display_token(lex->token));
                        }
                    } else {
                        compiler_diagf(stmt_loc, "Invalid follow up token after identifier %s", name);
                        return false;
                    }
                    lexer_get_and_expect_token(lex, TOKEN_SEMICOLON);
                } break;
            case TOKEN_EXTERN:
                {
                    lexer_get_and_expect_token(lex, TOKEN_ID);
                    Inst inst = (Inst) {
                        .loc = stmt_loc,
                        .kind = INST_EXTERN,
                        .a = MAKE_NAME_ARG(arena_strdup(fn->arena, lex->string)),
                    };
                    push_inst(fn, inst);
                    lexer_get_and_expect_token(lex, TOKEN_SEMICOLON);
                } break;
            default:
                {
                    compiler_diagf(stmt_loc, "Unexpected token %s to start a statement", lexer_display_token(lex->token));
                    return false;
                } break;
        }
    }

    if(!lexer_expect_token(lex, TOKEN_CCURLY)) return false;

    dump_function(fn);
    if(!generate_fasm_x86_64_win32_function(output, fn)) {
        compiler_diagf(top_loc, "Failed to compile function %s", fn->name);
        return false;
    }

    return true;
}

bool compile_program(Lexer *lex)
{
    Nob_String_Builder output = {0};
    Arena arena = {0};
    Scope scope = {0};

    generate_fasm_x86_64_win32_program_prolog(&output);

    while(lexer_get_token(lex) && lex->token != TOKEN_EOF) {
        Function fn = {0};
        fn.arena = &arena;
        if(!compile_function(&fn, lex, &scope, &output)) {
            nob_da_free(output);
            nob_da_free(scope);
            destroy_function_blocks(&fn);
            return false;
        }
        destroy_function_blocks(&fn);
    }

    if(!lexer_get_and_expect_token(lex, TOKEN_EOF)) return -1;
    generate_fasm_x86_64_win32_program_epilog(&output);
    if(!nob_write_entire_file("a.s", output.items, output.count)) return -1;
    nob_da_free(scope);
    nob_da_free(output);
    return true;
}

int main(int argc, char **argv)
{
    const char *program = nob_shift(argv, argc);
    if(argc <= 0) {
        fprintf(stderr, "error: provide an input file\n");
        fprintf(stderr, "usage: %s <input>\n", program);
        return -1;
    }

    char *input = nob_shift(argv, argc);
    Nob_String_Builder input_data = {0};
    if(!nob_read_entire_file(input, &input_data)) {
        fprintf(stderr, "error: invalid input file %s\n", input);
    }

    Lexer lex = lexer_new(input, input_data.items, input_data.items + input_data.count);
    if(!compile_program(&lex)) {
        fprintf(stderr, "Compilation failure\n");
    }
    return 0;
}
