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

#ifndef ARRAY_LEN
#define ARRAY_LEN(xs) (sizeof(xs)/sizeof(*(xs)))
#endif

typedef struct {
    char *input_path;
    int line_offset;
    int line_number;
} Loc;

#define compiler_missingf(loc, ...) _compiler_missingf(__FILE__, __LINE__, loc, __VA_ARGS__)
void _compiler_missingf(const char *file, int line, Loc loc, const char *fmt, ...);
void compiler_diagf(Loc loc, const char *fmt, ...);

// TODO: instead of registering tokens like this maybe 
//       having the user to register the tokens would be 
//       then I can use the lexer for multiple programming
//       language.
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
    X(EXTERN       , "extern"    ) \
    X(CASE         , "case"      ) \
    X(IF           , "if"        ) \
    X(ELSE         , "else"      ) \
    X(WHILE        , "while"     ) \
    X(SWITCH       , "switch"    ) \
    X(GOTO         , "goto"      ) \
    X(RETURN       , "return"    ) \
    X(FUNCTION     , "function"  ) \
    X(VAR          , "var"       ) \

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
    char *current;
    char *line_start;
    size_t line_number;
} ParsePoint;

typedef struct {
    int token;
    const char *literal;
    bool is_keyword;
} TokenInfo;

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

    TokenInfo tokens[128];
    size_t count_tokens;
} Lexer;

Lexer lexer_new(char *input_path, char *input_stream, char *eof);
void lexer_destroy(Lexer *lex);
bool lexer_get_token(Lexer *lex);
bool lexer_expect_token(Lexer *lex, Token token);
Token lexer_expect_token2(Lexer *lex, Token token_1, Token token_2);
bool lexer_get_and_expect_token(Lexer *lex, Token token);
const char *lexer_display_token(Token token);
Loc lexer_loc(Lexer *lex);

// TODO: let user register their own tokens
// void lexer_register_punct(Lexer *lex, const char *punct, int token);
// void lexer_register_keyword(Lexer *lex, const char *keyword, int token);
// void lexer_register_default_tokens(Lexer *lex); // C tokens

#endif // LEXER_H_
