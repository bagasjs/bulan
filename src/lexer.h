/*
 * This is a C rewrite of the lexer from:
 * https://github.com/tsoding/b/blob/main/src/lexer.rs
 *
 * Original author: Tsoding
 * Licensed under the MIT License
 * 
 * Copyright 2025 Alexey Kutepov <reximkut@gmail.com> and the Contributors
 * See LICENSE for the full license text.
 */

#ifndef LEXER_H_
#define LEXER_H_

#include <stdint.h>
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>

#define PUNCT_TOKEN_LIST    \
    X(QUESTION     , "?"  ) \
    X(OCURLY       , "{"  ) \
    X(CCURLY       , "}"  ) \
    X(OPAREN       , "("  ) \
    X(CPAREN       , ")"  ) \
    X(OBRACKET     , "["  ) \
    X(CBRACKET     , "]"  ) \
    X(SEMICOLON    , ";"  ) \
    X(COLON        , ":"  ) \
    X(COMMA        , ","  ) \
    X(MINUSMINUS   , "--" ) \
    X(MINUSEQ      , "-=" ) \
    X(MINUS        , "-"  ) \
    X(PLUSPLUS     , "++" ) \
    X(PLUSEQ       , "+=" ) \
    X(PLUS         , "+"  ) \
    X(MULEQ        , "*=" ) \
    X(MUL          , "*"  ) \
    X(MODEQ        , "%=" ) \
    X(MOD          , "%"  ) \
    X(DIVEQ        , "/=" ) \
    X(DIV          , "/"  ) \
    X(OREQ         , "|=" ) \
    X(OR           , "|"  ) \
    X(ANDEQ        , "&=" ) \
    X(AND          , "&"  ) \
    X(EQEQ         , "==" ) \
    X(EQ           , "="  ) \
    X(NOTEQ        , "!=" ) \
    X(NOT          , "!"  ) \
    X(SHLEQ        , "<<=") \
    X(SHL          , "<<" ) \
    X(LESSEQ       , "<=" ) \
    X(LESS         , "<"  ) \
    X(SHREQ        , ">>=") \
    X(SHR          , ">>" ) \
    X(GREATEREQ    , ">=" ) \
    X(GREATER      , ">"  ) \

#define KEYWORD_TOKEN_LIST         \
    X(VAR          , "var"       ) \
    X(EXTERN       , "extern"    ) \
    X(CASE         , "case"      ) \
    X(IF           , "if"        ) \
    X(ELSE         , "else"      ) \
    X(WHILE        , "while"     ) \
    X(SWITCH       , "switch"    ) \
    X(GOTO         , "goto"      ) \
    X(RETURN       , "return"    ) \
    X(FUNCTION     , "function"  ) \

typedef enum {
    // Terminal
    TOKEN_EOF = 0,
    TOKEN_PARSING_ERROR,

    // Values
    TOKEN_ID,
    TOKEN_STRING_LIT,
    TOKEN_CHAR_LIT,
    TOKEN_INT_LIT,
    TOKEN_FLOAT_LIT,

#define X(TOK, STR) TOKEN_##TOK,
    // Punctuations
    PUNCT_TOKEN_LIST
    // Keywords 
    KEYWORD_TOKEN_LIST
#undef X
} Token;

typedef struct {
    char *input_path;
    int line_offset;
    int line_number;
} Loc;

typedef struct {
    char *current;
    char *line_start;
    size_t line_number;
} ParsePoint;

typedef struct {
    char *input_path;
    char *input_stream;
    char *eof;
    ParsePoint parse_point;
    struct {
        char *items;
        size_t count;
        size_t capacity;
    } string_storage;
    Token token;
    char *string;
    int64_t int_number;
    double real_number;
    Loc loc;
} Lexer;

#define compiler_missingf(loc, ...) _compiler_missingf(__FILE__, __LINE__, loc, __VA_ARGS__)
void _compiler_missingf(const char *file, int line, Loc loc, const char *fmt, ...);
void compiler_diagf(Loc loc, const char *fmt, ...);

Lexer lexer_new(char *input_path, char *input_stream, char *eof);
bool lexer_get_token(Lexer *lex);
bool lexer_expect_token(Lexer *lex, Token token);
Token lexer_expect_token2(Lexer *lex, Token token_1, Token token_2);
bool lexer_get_and_expect_token(Lexer *lex, Token token);
const char *lexer_display_token(Token token);
Loc lexer_loc(Lexer *lex);

#endif // LEXER_H_
