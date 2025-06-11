#include <stdio.h>
#include <string.h>
#include "nob.h"
#include "arena.h"
#include "lexer.h"

typedef enum {
    BINDING_INVALID = 0,
    BINDING_VAR,
    BINDING_EXTERN,
} BindingKind;

typedef struct {
    BindingKind kind;
    const char *name;
    union {
        size_t as_var;
    };
} Binding;

typedef struct {
    const char *name;
} Extern;

typedef struct {
    size_t var_count;
    Binding *items;
    size_t count;
    size_t capacity;
} Scope;

Binding *scope_find(Scope scope, const char *name)
{
    for(size_t i = 0; i < scope.count; ++i) {
        if(strcmp(scope.items[i].name, name) == 0) {
            return &scope.items[i];
        }
    }
    return NULL;
}

Binding *scope_alloc(Scope *scope, const char *name)
{
    Binding b;
    b.name = name;
    b.kind = BINDING_INVALID;
    nob_da_append(scope, b);
    return &scope->items[scope->count - 1];
}

Binding *scope_alloc_var(Scope *scope, const char *name)
{
    Binding *b = scope_alloc(scope, name);
    b->kind = BINDING_VAR;
    b->as_var = scope->var_count;
    scope->var_count += 1;
    return b;
}

bool compile_program(Lexer *lex)
{
    Arena arena = {0};
    Scope scope = {0};
    Nob_String_Builder output = {0};
    if(!lexer_get_and_expect_token(lex, TOKEN_FUNCTION)) return -1;

    if(!lexer_get_and_expect_token(lex, TOKEN_ID)) return -1;
    nob_sb_appendf(&output, "format MS64 COFF\n");
    nob_sb_appendf(&output, "section \".text\" executable\n");
    nob_sb_appendf(&output, "public _%s as '%s'\n", lex->string, lex->string);
    nob_sb_appendf(&output, "_%s:\n", lex->string);
    nob_sb_appendf(&output, "    push rbp\n");
    nob_sb_appendf(&output, "    mov  rbp, rsp\n");

    if(!lexer_get_and_expect_token(lex, TOKEN_OPAREN)) return -1;
    if(!lexer_get_and_expect_token(lex, TOKEN_CPAREN)) return -1;
    if(!lexer_get_and_expect_token(lex, TOKEN_OCURLY)) return -1;

    while(lexer_get_token(lex) && lex->token != TOKEN_CCURLY) {
        Loc stmt_loc = lex->loc;
        switch(lex->token) {
            case TOKEN_VAR:
                {
                    lexer_get_and_expect_token(lex, TOKEN_ID);
                    Binding *var = scope_alloc_var(&scope, arena_strdup(&arena, lex->string));
                    nob_sb_appendf(&output, "    sub rsp, 8\n");
                    nob_sb_appendf(&output, "    mov QWORD [rbp - %zu], 0\n", (var->as_var + 1) * 8);
                    lexer_get_and_expect_token(lex, TOKEN_SEMICOLON);
                } break;
            case TOKEN_ID:
                {
                    char *name = arena_strdup(&arena, lex->string);
                    lexer_get_token(lex);
                    if(lex->token == TOKEN_EQ) {
                        Binding *var = scope_find(scope, name);
                        if(var == NULL) {
                            compiler_diagf(lex->loc, "Could not find %s in scope", name);
                            return false;
                        }
                        lexer_get_and_expect_token(lex, TOKEN_INT_LIT);
                        nob_sb_appendf(&output, "    mov QWORD [rbp - %zu], %llu\n", (var->as_var+ 1) * 8, lex->int_number);
                    } else if (lex->token == TOKEN_OPAREN) {
                        lexer_get_and_expect_token(lex, TOKEN_ID);
                        Binding *arg = scope_find(scope, lex->string);
                        if(arg == NULL) {
                            compiler_diagf(lex->loc, "Could not find %s in scope", lex->string);
                            return false;
                        }
                        if(arg->kind != BINDING_VAR) {
                            compiler_diagf(lex->loc, "Expecting function argument %s to be a variable", lex->string);
                            return false;
                        }
                        nob_sb_appendf(&output, "    mov  rcx, QWORD [rbp - %zu]\n",  (arg->as_var+ 1) * 8);
                        nob_sb_appendf(&output, "    call %s\n", name);
                        lexer_get_and_expect_token(lex, TOKEN_CPAREN);
                    } else {
                        compiler_diagf(stmt_loc, "Invalid follow up token after identifier %s", name);
                    }
                    lexer_get_and_expect_token(lex, TOKEN_SEMICOLON);
                } break;
            case TOKEN_EXTERN:
                {
                    lexer_get_and_expect_token(lex, TOKEN_ID);
                    Binding *extrnl = scope_alloc(&scope, arena_strdup(&arena, lex->string));
                    nob_sb_appendf(&output, "    extrn %s\n", extrnl->name);
                    lexer_get_and_expect_token(lex, TOKEN_SEMICOLON);
                } break;
            default:
                {
                } break;
        }
    }

    if(!lexer_expect_token(lex, TOKEN_CCURLY)) return -1;
    if(!lexer_get_and_expect_token(lex, TOKEN_EOF)) return -1;
    nob_sb_appendf(&output, "    mov rsp, rbp\n");
    nob_sb_appendf(&output, "    pop rbp\n");
    nob_sb_appendf(&output, "    ret\n");
    nob_da_free(scope);

    if(!nob_write_entire_file("a.S", output.items, output.count)) return -1;
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
