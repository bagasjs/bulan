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

typedef struct Compiler {
    Arena arena;
    Target target;
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

Var *find_var(const Compiler *com, const char *name)
{
    for(size_t i = 0; i < com->vars.count; ++i) {
        if(strcmp(com->vars.items[i].name, name) == 0) {
            return &com->vars.items[i];
        }
    }
    return NULL;
}

Var *alloc_var(Compiler *com, const char *name)
{
    Var b = (Var){ .name = name, .index = com->vars.count };
    nob_da_append(&com->vars, b);
    return &com->vars.items[com->vars.count - 1];
}

Var *alloc_var_local(Compiler *com, const char *name, size_t index)
{
    Var *local = alloc_var(com, name);
    local->name = name;
    local->storage = VAR_LOCAL;
    local->index = index;
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
                Var *var = find_var(com, lex->string);
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
        case TOKEN_PLUS:  return INST_ADD;
        case TOKEN_MINUS: return INST_SUB;
        case TOKEN_LESS:  return INST_LT;
        case TOKEN_LESSEQ:  return INST_LE;
        case TOKEN_GREATER:  return INST_GT;
        case TOKEN_GREATEREQ:  return INST_GE;
        case TOKEN_EQEQ:  return INST_EQ;
        case TOKEN_NOTEQ:  return INST_NE;
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
        Arg result = MAKE_LOCAL_INDEX_ARG(alloc_local(fn));
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

bool compile_block(Compiler *com, Function *fn, Lexer *lex)
{
    while(lexer_get_token(lex) && lex->token != TOKEN_CCURLY) {
        Loc stmt_loc = lex->loc;
        switch(lex->token) {
            case TOKEN_IF:
                {
                    Arg arg = {0};
                    if(!lexer_get_and_expect_token(lex, TOKEN_OPAREN)) return false;
                    if(!compile_expression(&arg, fn, lex, com)) return false;
                    if(!lexer_get_and_expect_token(lex, TOKEN_CPAREN)) return false;
                    size_t then_label  = alloc_label(fn);
                    size_t next_label = alloc_label(fn);
                    Inst *inst = push_inst(fn, (Inst) {
                        .loc  = stmt_loc,
                        .kind = INST_BRANCH,
                        .args[0] = MAKE_LABEL_ARG(then_label),
                        .args[1] = MAKE_LABEL_ARG(next_label),
                        .args[2] = arg,
                    });
                    if(!lexer_get_and_expect_token(lex, TOKEN_OCURLY)) return false;
                    push_inst(fn, (Inst) {
                        .loc  = stmt_loc,
                        .kind = INST_LABEL,
                        .args[0] = MAKE_LABEL_ARG(then_label),
                    });
                    if(!compile_block(com, fn, lex)) return false;
                    ParsePoint saved_point = lex->parse_point;
                    if(!lexer_get_token(lex)) return false;
                    if(lex->token == TOKEN_ELSE) {
                        size_t end_label = alloc_label(fn);
                        push_inst(fn, (Inst) {
                            .loc  = stmt_loc,
                            .kind = INST_JMP,
                            .args[0] = MAKE_LABEL_ARG(end_label),
                        });
                        while(lex->token == TOKEN_ELSE) {
                            push_inst(fn, (Inst) {
                                .loc  = stmt_loc,
                                .kind = INST_LABEL,
                                .args[0] = MAKE_LABEL_ARG(next_label),
                            });
                            if(!lexer_get_token(lex)) return false;
                            bool should_break = true;
                            if(lex->token == TOKEN_IF) {
                                then_label = alloc_label(fn);
                                next_label = alloc_label(fn);
                                should_break = false;
                                if(!lexer_get_and_expect_token(lex, TOKEN_OPAREN)) return false;
                                if(!compile_expression(&arg, fn, lex, com)) return false;
                                if(!lexer_get_and_expect_token(lex, TOKEN_CPAREN)) return false;
                                Inst *inst = push_inst(fn, (Inst) {
                                    .loc  = stmt_loc,
                                    .kind = INST_BRANCH,
                                    .args[0] = MAKE_LABEL_ARG(then_label),
                                    .args[1] = MAKE_LABEL_ARG(next_label),
                                    .args[2] = arg,
                                });
                                if(!lexer_get_token(lex)) return false;
                                push_inst(fn, (Inst) {
                                    .loc  = stmt_loc,
                                    .kind = INST_LABEL,
                                    .args[0] = MAKE_LABEL_ARG(then_label),
                                });
                            }
                            if(!lexer_expect_token(lex, TOKEN_OCURLY)) return false;
                            if(!compile_block(com, fn, lex)) return false;
                            push_inst(fn, (Inst) {
                                .loc  = stmt_loc,
                                .kind = INST_JMP,
                                .args[0] = MAKE_LABEL_ARG(end_label),
                            });
                            if(should_break) break;
                            if(!lexer_get_token(lex)) return false;
                        }
                        push_inst(fn, (Inst) {
                            .loc  = stmt_loc,
                            .kind = INST_LABEL,
                            .args[0] = MAKE_LABEL_ARG(end_label),
                        });
                    } else {
                        lex->parse_point = saved_point;
                        push_inst(fn, (Inst) {
                            .loc  = stmt_loc,
                            .kind = INST_JMP,
                            .args[0] = MAKE_LABEL_ARG(next_label),
                        });
                        push_inst(fn, (Inst) {
                            .loc  = stmt_loc,
                            .kind = INST_LABEL,
                            .args[0] = MAKE_LABEL_ARG(next_label),
                        });
                    }
                } break;
            case TOKEN_WHILE:
                {
                    size_t start_label = fn->labels_count;
                    size_t body_label  = fn->labels_count + 1;
                    size_t end_label   = fn->labels_count + 2;
                    lexer_get_and_expect_token(lex, TOKEN_OPAREN);
                    push_inst(fn, (Inst) {
                        .loc  = stmt_loc,
                        .kind = INST_LABEL,
                        .args[0] = MAKE_LABEL_ARG(start_label),
                    });

                    Arg arg = {0};
                    if(!compile_expression(&arg, fn, lex, com)) return false;
                    if(!lexer_get_and_expect_token(lex, TOKEN_CPAREN)) return false;
                    Inst *inst = push_inst(fn, (Inst) {
                        .loc  = stmt_loc,
                        .kind = INST_BRANCH,
                        .args[0] = MAKE_LABEL_ARG(body_label),
                        .args[1] = MAKE_LABEL_ARG(end_label),
                        .args[2] = arg,
                    });
                    if(!lexer_get_and_expect_token(lex, TOKEN_OCURLY)) return false;
                    push_inst(fn, (Inst) {
                        .loc  = stmt_loc,
                        .kind = INST_LABEL,
                        .args[0] = MAKE_LABEL_ARG(body_label),
                    });
                    if(!compile_block(com, fn, lex)) return false;
                    push_inst(fn, (Inst) {
                        .loc  = stmt_loc,
                        .kind = INST_JMP,
                        .args[0] = MAKE_LABEL_ARG(start_label),
                    });
                    push_inst(fn, (Inst) {
                        .loc  = stmt_loc,
                        .kind = INST_LABEL,
                        .args[0] = MAKE_LABEL_ARG(end_label),
                    });
                    fn->labels_count += 3;
                } break;
            case TOKEN_ID:
                {
                    char *name = arena_strdup(&com->arena, lex->string);
                    lexer_get_token(lex);
                    if(lex->token == TOKEN_EQ) {
                        // Assignment
                        Var *var = find_var(com, name);
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
                        ArgList args = {0};
                        ParsePoint saved_point = lex->parse_point;
                        lexer_get_token(lex);
                        while(true) {
                            if(lex->token == TOKEN_CPAREN) break;
                            lex->parse_point = saved_point;
                            if(!compile_expression(&b, fn, lex, com)) return false;
                            arena_da_append(&com->arena, &args, b);
                            lexer_get_token(lex);
                            if(lex->token == TOKEN_CPAREN) break;
                            if(!lexer_expect_token(lex, TOKEN_COMMA)) return false;
                            saved_point = lex->parse_point;
                            lexer_get_token(lex);
                        }
                        lexer_expect_token(lex, TOKEN_CPAREN);

                        push_inst(fn, (Inst){
                            .loc = stmt_loc,
                            .kind = INST_FUNCALL,
                            .args[0] = MAKE_NAME_ARG(name),
                            .args[1] = MAKE_LIST_ARG(args),
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
                        .args[0] = MAKE_NAME_ARG(arena_strdup(&com->arena, lex->string)),
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

    return true;
}

bool compile_function(Compiler *com, Function *fn, Lexer *lex, Nob_String_Builder *output)
{
    fn->loc = lex->loc;
    com->vars.count = 0;
    if(!lexer_expect_token(lex, TOKEN_FUNCTION)) return false;
    if(!lexer_get_and_expect_token(lex, TOKEN_ID)) return false;
    fn->name  = arena_strdup(&com->arena, lex->string);

    if(!lexer_get_and_expect_token(lex, TOKEN_OPAREN)) return false;
    if(!lexer_get_and_expect_token(lex, TOKEN_CPAREN)) return false;

    if(!lexer_get_token(lex)) return false;
    if(lex->token == TOKEN_COLON) {
        if(!lexer_get_and_expect_token(lex, TOKEN_ID)) return false;
        while(lex->token == TOKEN_ID) {
            if(find_var(com, lex->string) != NULL) {
                compiler_diagf(lex->loc, "Variable with name `%s` is already exists", lex->string);
                return false;
            }
            alloc_var_local(com, arena_strdup(&com->arena, lex->string), alloc_local(fn));
            lexer_get_token(lex);
            if(lex->token == TOKEN_COMMA) lexer_get_token(lex);
        }
    }

    if(!lexer_expect_token(lex, TOKEN_OCURLY)) return false;
    if(!compile_block(com, fn, lex)) return false;
    if(!lexer_expect_token(lex, TOKEN_CCURLY)) return false;
    return true;
}

bool compile_program(Compiler *com, Nob_String_Builder *output, Lexer *lex)
{
    bool ok = true;
    while(lexer_get_token(lex) && lex->token != TOKEN_EOF) {
        Function fn = {0};
        ok = compile_function(com, &fn, lex, output);
        if(!ok) break;
        nob_da_append(&com->funcs, fn);
    }
    if(!lexer_get_and_expect_token(lex, TOKEN_EOF)) return false;

    if(ok) {
        ok = ok && lexer_get_and_expect_token(lex, TOKEN_EOF);
        Program program = {0};
        program.target = com->target;
        program.funcs = com->funcs.items;
        program.count_funcs = com->funcs.count;
        program.static_data = com->static_data.items;
        program.count_static_data = com->static_data.count;
        ok = ok && generate_program(&program, output);
    }

    for(size_t i = 0; i < com->funcs.count; ++i) {
        Function fn = com->funcs.items[i];
        nob_da_free(fn);
    }

    nob_da_free(com->funcs);
    arena_free(&com->arena);
    return ok;
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
    Target target = parse_target(*target_str);

    char *input = nob_shift(rest_argv, rest_argc);

    Nob_String_Builder input_data = {0};
    if(!nob_read_entire_file(input, &input_data)) {
        fprintf(stderr, "error: invalid input file %s\n", input);
    }

    Compiler com = {0};
    Nob_String_Builder output = {0};
    Lexer lex = lexer_new(input, input_data.items, input_data.items + input_data.count);

    com.target = target;
    const char *output_filepath = "a.s";
    if(com.target == TARGET_HTML_JS) {
        output_filepath = "a.html";
    }

    if(!compile_program(&com, &output, &lex)) {
        fprintf(stderr, "Compilation failure\n");
    }
    if(!nob_write_entire_file(output_filepath, output.items, output.count)) return false;
    nob_da_free(output);
    nob_da_free(com.vars);

    return 0;
}

