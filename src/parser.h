#ifndef PARSER_H_
#define PARSER_H_

#include "fs.h"
#include "lexer.h"
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    SourceFile const* source;
    Tokens* tokens;
    Arena* arena;
    size_t pos;
} Parser;


typedef enum {
    ET_NUMBER,
    ET_BIN,
} ExprType;

typedef enum {
    ST_RET
} StmtType;

typedef struct Expr {
    ExprType type;
    union {
        uint64_t number;
        struct {
            struct Expr* l;
            struct Expr* r;
            OperatorType op;
        } bin;
    };
} Expr;

typedef struct Stmt {
    StmtType type;
    union {
        Expr ret;
    };
} Stmt;

typedef struct {
    Stmt* items;
    size_t count;
    size_t capacity;
} Body;

bool parser_parse(Parser* parser, Body* out);


#endif
