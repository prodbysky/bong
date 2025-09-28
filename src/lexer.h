#ifndef LEXER_H_
#define LEXER_H_

#include "fs.h"
#include "str.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    TT_NUMBER,
    TT_OPERATOR,
    TT_SEMI,
    TT_KEYWORD,
    TT_IDENT,
} TokenType;

typedef enum {
    OT_PLUS,
    OT_MINUS,
    OT_STAR,
    OT_SLASH,
} OperatorType;

typedef enum {
    KT_NO = 0,
    KT_RETURN,
} KeywordType;

typedef struct {
    TokenType type;
    SourceFile const * file;
    size_t offset;
    size_t len;
    union {
        uint64_t number;
        OperatorType op;
        KeywordType kw;
        StringView id;
    };
} Token;

typedef struct {
    Token* items;
    size_t count;
    size_t capacity;
} Tokens;

typedef struct {
    SourceFile const* source;
    Arena* arena;
    size_t pos;
} Lexer;

void print_token(const Token* t);
bool lexer_run(Lexer* lexer, Tokens* out);



#endif
