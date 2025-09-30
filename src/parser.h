#ifndef PARSER_H_
#define PARSER_H_

#include "fs.h"
#include "lexer.h"
#include "str.h"
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
    ET_ID,
    ET_BIN,
} ExprType;

typedef enum {
    ST_IF,
    ST_RET,
    ST_VAR_DEF,
    ST_VAR_REASSIGN,
} StmtType;

typedef struct Expr {
    ExprType type;
    union {
        uint64_t number;
        StringView id;
        struct {
            struct Expr* l;
            struct Expr* r;
            OperatorType op;
        } bin;
    };
} Expr;


struct Stmt;

typedef struct {
    struct Stmt* items;
    size_t count;
    size_t capacity;
} Body;

typedef struct Stmt {
    StmtType type;
    union {
        Expr ret;
        struct {
            Expr cond;
            Body body;
        } if_st;
        struct {
            StringView name;
            Expr value;
        } var_def;
        struct {
            StringView name;
            Expr value;
        } var_reassign;
    };
} Stmt;

bool parser_parse(Parser* parser, Body* out);

#endif
