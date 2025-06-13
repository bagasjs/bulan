#include <stdio.h>
#include <string.h>
#include "nob.h"
#include "arena.h"
#include "lexer.h"
#include "codegen.h"
#include "flag.h"

typedef enum {
    VAR_LOCAL  = 0,
    VAR_EXTERN,
} VarStorage;

typedef struct {
    const char *name;
    size_t index;
    VarStorage storage;
} Var;

typedef struct Scope Scope;
struct Scope {
    size_t vars_count;
    Var *items;
    size_t count;
    size_t capacity;
};

typedef struct Compiler {
    Arena arena;
    Target target;
    Scope scope;
    Function func;
    Nob_String_Builder static_data;
} Compiler;

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

Var *scope_alloc_local(Scope *scope, const char *name)
{
    Var *local = scope_alloc(scope, name);
    local->name = name;
    local->storage = VAR_LOCAL;
    local->index = scope->vars_count;
    scope->vars_count += 1;
    return local;
}

bool compile_primary_expression(Arg *result_arg, Function *fn, Lexer *lex, Compiler *com) 
{
    assert(result_arg);
    lexer_get_token(lex);
    switch(lex->token) {
        case TOKEN_INT_LIT:
            *result_arg = MAKE_INT_VALUE_ARG(lex->int_number);
            return true;
        case TOKEN_STRING_LIT:
            *result_arg = MAKE_STATIC_DATA_ARG(com->static_data.count);
            nob_sb_append_cstr(&com->static_data, lex->string);
            nob_da_append(&com->static_data, 0);
            return true;
        case TOKEN_ID:
            {
                Var *var = scope_find(&com->scope, lex->string);
                if(var == NULL) {
                    compiler_diagf(lex->loc, "Could not find %s in scope", lex->string);
                    return false;
                }
                if(var->storage != VAR_LOCAL) {
                    compiler_diagf(lex->loc, "Variable %s is not a local variable", var->name);
                    return false;
                }
                *result_arg = MAKE_LOCAL_INDEX_ARG(var->index);
                return true;
            }
        default:
            compiler_diagf(lex->loc, "Invalid token %s to start an expression", lexer_display_token(lex->token));
    }
    return false;
}

InstKind token_to_binop_inst_kind(Token token)
{
    switch(token) {
        case TOKEN_PLUS: return INST_ADD;
        case TOKEN_MINUS: return INST_SUB;
        default: return INST_NOP;
    }
}

bool compile_expression(Arg *result_arg, Function *fn, Lexer *lex, Compiler *com)
{
    Arg lhs = {0};
    if(!compile_primary_expression(&lhs, fn, lex, com)) return false;

    ParsePoint saved_point = lex->parse_point;
    lexer_get_token(lex);
    InstKind inst_kind = token_to_binop_inst_kind(lex->token);
    if(inst_kind != INST_NOP) {
        Arg result = MAKE_LOCAL_INDEX_ARG(com->scope.vars_count);
        push_inst(fn, (Inst) {
            .kind = INST_LOCAL_INIT,
            .args[0] = result,
        });
        com->scope.vars_count += 1;

        while((inst_kind = token_to_binop_inst_kind(lex->token)) != INST_NOP) {
            Arg rhs = {0};
            if(!compile_primary_expression(&rhs, fn, lex, com)) return false;
            push_inst(fn, (Inst) {
                .kind = inst_kind,
                .args[0] = result,
                .args[1] = lhs,
                .args[2] = rhs,
            });
            lhs = result;
            saved_point = lex->parse_point;
            lexer_get_token(lex);
        }
        *result_arg = result;
    } else {
        *result_arg = lhs;
    }
    lex->parse_point = saved_point;

    return true;
}

bool compile_function(Compiler *com, Function *fn, Lexer *lex, Nob_String_Builder *output)
{
    Loc top_loc = lex->loc;
    com->scope.count = 0;
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
                    if(scope_find(&com->scope, lex->string) != NULL) {
                        compiler_diagf(lex->loc, "Variable with name `%s` is already exists", lex->string);
                        return false;
                    }
                    Var *var = scope_alloc_local(&com->scope, arena_strdup(fn->arena, lex->string));
                    push_inst(fn, (Inst) {
                        .loc = stmt_loc,
                        .kind = INST_LOCAL_INIT,
                        .args[0] = MAKE_LOCAL_INDEX_ARG(var->index),
                    });
                    lexer_get_and_expect_token(lex, TOKEN_SEMICOLON);
                } break;
            case TOKEN_ID:
                {
                    char *name = arena_strdup(fn->arena, lex->string);
                    lexer_get_token(lex);
                    if(lex->token == TOKEN_EQ) {
                        // Assignment
                        Var *var = scope_find(&com->scope, name);
                        if(var == NULL) {
                            compiler_diagf(lex->loc, "Could not find %s in scope", name);
                            return false;
                        }
                        if(var->storage != VAR_LOCAL) {
                            compiler_diagf(lex->loc, "Variable %s is not a local variable", name);
                            return false;
                        }
                        Arg b = {0};
                        if(!compile_expression(&b, fn, lex, com)) return false;
                        push_inst(fn, (Inst){
                            .loc = stmt_loc,
                            .kind = INST_LOCAL_ASSIGN,
                            .args[0] = MAKE_LOCAL_INDEX_ARG(var->index),
                            .args[1] = b,
                        });
                    } else if (lex->token == TOKEN_OPAREN) {
                        // Function call
                        Arg b = {0};
                        ParsePoint saved_point = lex->parse_point;
                        lexer_get_token(lex);
                        if(lex->token != TOKEN_CPAREN) {
                            lex->parse_point = saved_point;
                            if(!compile_expression(&b, fn, lex, com)) return false;
                            lexer_get_and_expect_token(lex, TOKEN_CPAREN);
                        }
                        push_inst(fn, (Inst){
                            .loc = stmt_loc,
                            .kind = INST_FUNCALL,
                            .args[0] = MAKE_NAME_ARG(name),
                            .args[1] = b,
                        });
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
                        .args[0] = MAKE_NAME_ARG(arena_strdup(fn->arena, lex->string)),
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

    if(!generate_function(com->target, output, fn)) {
        compiler_diagf(top_loc, "Failed to compile function %s", fn->name);
        return false;
    }

    return true;
}

bool compile_program(Compiler *com, Nob_String_Builder *output, Lexer *lex)
{
    generate_program_prolog(com->target, output);
    while(lexer_get_token(lex) && lex->token != TOKEN_EOF) {
        Function fn = {0};
        fn.arena = &com->arena;
        bool ok = compile_function(com, &fn, lex, output);
        destroy_function_blocks(&fn);
        if(!ok) return false;
    }
    if(!lexer_get_and_expect_token(lex, TOKEN_EOF)) return false;
    generate_program_epilog(com->target, output);
    generate_static_data(com->target, output, com->static_data);
    return true;
}


void usage(FILE *stream)
{
    fprintf(stream, "Usage: ./blnc [OPTIONS] [--] <OUTPUT FILES...>\n");
    fprintf(stream, "OPTIONS:\n");
    flag_print_options(stream);
}

#define DEFAULT_TARGET TARGET_FASM_X86_64_WIN32
Target parse_target(const char *target_str)
{
    if(!target_str) return DEFAULT_TARGET;
    if(strcmp(target_str, "list") == 0) {
        printf("Available target:\n");
        for(Target t = 1; t < _COUNT_TARGETS; ++t) {
            printf("    %s\n", display_target(t));
        }
        exit(0);
    }
    for(Target t = 1; t < _COUNT_TARGETS; ++t) {
        if(strcmp(target_str, display_target(t)) == 0)  {
            return t;
        }
    }
    fprintf(stderr, "ERROR: please provide a valid target. You gave %s\n", target_str);
    exit(-1);
}

int main(int argc, char **argv)
{
    bool *help = flag_bool("help", false, "Print this help to stdout");
    char **target_str = flag_str("t", NULL, "Target platform to compilation");

    if(!flag_parse(argc, argv)) {
        usage(stderr);
        flag_print_error(stderr);
        return -1;
    }

    if(*help) {
        usage(stdout);
        return 0; 
    }

    int rest_argc = flag_rest_argc();
    char **rest_argv = flag_rest_argv();

    char *input = nob_shift(rest_argv, rest_argc);

    Nob_String_Builder input_data = {0};
    if(!nob_read_entire_file(input, &input_data)) {
        fprintf(stderr, "error: invalid input file %s\n", input);
    }

    Compiler com = {0};
    Nob_String_Builder output = {0};
    Lexer lex = lexer_new(input, input_data.items, input_data.items + input_data.count);

    com.target = parse_target(*target_str);
    const char *output_filepath = "a.s";
    if(com.target == TARGET_HTML_JS) {
        output_filepath = "a.html";
    }

    if(!compile_program(&com, &output, &lex)) {
        fprintf(stderr, "Compilation failure\n");
    }
    if(!nob_write_entire_file(output_filepath, output.items, output.count)) return false;
    nob_da_free(output);
    nob_da_free(com.scope);

    return 0;
}
