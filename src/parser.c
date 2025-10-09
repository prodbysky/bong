#include "parser.h"
#include "da.h"
#include <stdio.h>
#include "error.h"
#include "lexer.h"
#include "str.h"
#include <assert.h>

static bool __peek(const Parser* parser, Token* out);
static bool __bump(Parser* parser, Token* out);
static bool __expect_and_bump(Parser* parser, TokenType type, Token* out);
static bool __empty(const Parser* parser);

static bool __stmt(Parser* parser, Stmt* out);
static bool __block(Parser* parser, Body* out);
/*
    Ref: Crafting interpreters page 80
    expression → equality ;
    equality → comparison ( ( "!=" | "==" ) comparison )* ;
    comparison → term ( ( ">" | ">=" | "<" | "<=" ) term )* ;
    term → factor ( ( "-" | "+" ) factor )* ;
    factor → unary ( ( "/" | "*" ) unary )* ;
    unary → ( "!" | "-" ) unary
    | primary ;
    primary → NUMBER | STRING | "true" | "false"
    | "(" expression ")" ;
*/
static bool __expression(Parser* parser, Expr* out);
static bool __eq(Parser* parser, Expr* out);
static bool __cmp(Parser* parser, Expr* out);
static bool __term(Parser* parser, Expr* out);
static bool __factor(Parser* parser, Expr* out);
static bool __unary(Parser* parser, Expr* out);
static bool __primary(Parser* parser, Expr* out);
static Token __last_token(const Parser* parser);

static bool __type_name(Parser* parser, TypeName* out);

static bool __stmt(Parser* parser, Stmt* out) {
    Token curr;
    if (!__bump(parser, &curr)) {
        fprintf(stderr, "[ERROR]: Missing keyword for statment\n");
        bong_error(parser->source, __last_token(parser).offset);
        return false;
    }

    switch (curr.type) {
        case TT_KEYWORD: {
            switch (curr.kw) {
                case KT_NO: assert(false);
                case KT_RETURN: {
                    Expr e;
                    if (!__expression(parser, &e)) return false;
                    out->type = ST_RET;
                    out->ret = e;
                    Token hopefully_semi = {0};
                    if (!__expect_and_bump(parser, TT_SEMI, &hopefully_semi)) {
                        fprintf(stderr, "[ERROR]: Missing statment termination semicolon\n");
                        bong_error(parser->source, hopefully_semi.offset);
                        return false;
                    }
                    return true;
                }
                case KT_IF: {
                    out->type = ST_IF;
                    if (!__expression(parser, &out->if_st.cond)) return false;
                    if (!__block(parser, &out->if_st.body)) return false;
                    return true;
                }
                case KT_WHILE: {
                    out->type = ST_WHILE;
                    if (!__expression(parser, &out->while_st.cond)) return false;
                    if (!__block(parser, &out->while_st.body)) return false;
                    return true;
                }
            }
        }
        case TT_IDENT: {
            Token name = curr;
            if (!__bump(parser, &curr)) return false;
            switch (curr.type) {
                case TT_COLON: {
                    out->type = ST_VAR_DEF;
                    if (!__type_name(parser, &out->var_def.type)) return false;
                    if (!__expect_and_bump(parser, TT_ASSIGN, &curr)) {
                        fprintf(stderr, "[ERROR]: After a bare identifier a [colon]-assign token is expected\n");
                        bong_error(parser->source, __last_token(parser).offset);
                        return false;
                    }
                    Expr e = {0};
                    if (!__expression(parser, &e)) return false;
                    if (!__expect_and_bump(parser, TT_SEMI, &curr)) {
                        fprintf(stderr, "[ERROR]: A semicolon is expected after the expression of the var define statement\n");
                        bong_error(parser->source, __last_token(parser).offset);
                        return false;
                    }
                    out->var_def.name = name.id;
                    out->var_def.value = e;
                    return true;
                }
                case TT_ASSIGN: {
                    Expr e = {0};
                    if (!__expression(parser, &e)) return false;
                    out->type = ST_VAR_REASSIGN;
                    out->var_reassign.name = name.id;
                    out->var_reassign.value = e;
                    if (!__expect_and_bump(parser, TT_SEMI, &curr)) {
                        fprintf(stderr, "[ERROR]: A semicolon is expected after the expression of the var define statement\n");
                        bong_error(parser->source, __last_token(parser).offset);
                        return false;
                    }
                    return true;
                }
                default: {
                    fprintf(stderr, "[ERROR]: Unknown token after an identifier in a statement\n");
                    bong_error(parser->source, curr.offset);
                    return false;
                }
            }
        }
        default: {
            fprintf(stderr, "[ERROR]: Unknown token at the beginning of a statement\n");
            bong_error(parser->source, curr.offset);
            return false;
        }
    }
    assert(false);
}

static bool __block(Parser* parser, Body* out) {
    Token open_curly;
    if (!__expect_and_bump(parser, TT_OPEN_CURLY, &open_curly)) {
        fprintf(stderr, "[ERROR]: Expected a `{` to open a block\n");
        bong_error(parser->source, open_curly.offset);
        return false;
    }
    while (__peek(parser, &open_curly)) {
        if (open_curly.type == TT_CLOSE_CURLY) {
            __bump(parser, &open_curly);
            return true;
        }
        Stmt s = {0};
        if (!__stmt(parser, &s)) return false;
        da_push(out, s, parser->arena);
    }
    fprintf(stderr, "[ERROR]: Missing `}` to close a block\n");
    bong_error(parser->source, open_curly.offset);
    return false;
}

static bool __type_name(Parser* parser, TypeName* out) {
    Token t;
    if (!__bump(parser, &t)) return false;
    switch (t.type) {
        case TT_IDENT: {
            if (t.id.count == 3 && strncmp(t.id.items, "u64", 3) == 0) {
                out->type = TNT_PRIMITIVE;
                out->primitive = PT_U64;
                return true;
            }
            fprintf(stderr, "[ERROR]: Unknown type name %.*s found\n", (int)t.id.count, t.id.items);
            bong_error(parser->source, t.offset);
            return false;
        }
        default: {
            fprintf(stderr, "[ERROR]: Unexpected token found in place of a type name\n");
            bong_error(parser->source, t.offset);
            return false;
        }
    }
    return false;
}


static bool __expression(Parser* parser, Expr* out) {
    return __eq(parser, out);
}

static bool __eq(Parser* parser, Expr* out) {
    return __cmp(parser, out);
}

static bool __cmp(Parser* parser, Expr* out) {
    if (!__term(parser, out)) return false;
    Token t = {0};
    while (!__empty(parser) && __peek(parser, &t) && (t.type == TT_OPERATOR && (t.op == OT_LT || t.op == OT_MT))) {
        __bump(parser, &t);
        Expr* left = arena_alloc(parser->arena, sizeof(Expr));
        *left = *out;
        out->type = ET_BIN;
        out->bin.l = left;
        out->bin.op = t.op;
        out->bin.r = arena_alloc(parser->arena, sizeof(Expr));
        if (!__term(parser, out->bin.r)) return false;
    }
    return true;
}

static bool __term(Parser* parser, Expr* out) {
    if (!__factor(parser, out)) return false;
    Token t = {0};
    while (!__empty(parser) && __peek(parser, &t) && (t.type == TT_OPERATOR && (t.op == OT_PLUS || t.op == OT_MINUS))) {
        __bump(parser, &t);
        Expr* left = arena_alloc(parser->arena, sizeof(Expr));
        *left = *out;
        out->type = ET_BIN;
        out->bin.l = left;
        out->bin.op = t.op;
        out->bin.r = arena_alloc(parser->arena, sizeof(Expr));
        if (!__factor(parser, out->bin.r)) return false;
    }
    return true;
}

static bool __factor(Parser* parser, Expr* out) {
    if (!__unary(parser, out)) return false;
    Token t = {0};
    while (!__empty(parser) && __peek(parser, &t) && (t.type == TT_OPERATOR && (t.op == OT_STAR || t.op == OT_SLASH))) {
        __bump(parser, &t);
        Expr* left = arena_alloc(parser->arena, sizeof(Expr));
        *left = *out;
        out->type = ET_BIN;
        out->bin.l = left;
        out->bin.op = t.op;
        out->bin.r = arena_alloc(parser->arena, sizeof(Expr));
        if (!__unary(parser, out->bin.r)) return false;
    }
    return true;
}
static bool __unary(Parser* parser, Expr* out) {
    return __primary(parser, out);
}
static bool __primary(Parser* parser, Expr* out) {
    Token t = {0};
    if (!__bump(parser, &t)) {
        fprintf(stderr, "[ERROR]: Missing expression\n");
        return false;
    }
    switch (t.type) {
        case TT_NUMBER: {
            out->type = ET_NUMBER;
            out->number = t.number;
            return true;
        }
        case TT_IDENT: {
            // TODO: Function calls as values 
            out->type = ET_ID;
            out->id = t.id;
            return true;
        }
        default: {
            fprintf(stderr, "[ERROR]: Unexpected token in place of primary expression %d\n", t.type);
            bong_error(parser->source, t.offset);
            return false;
        }
    }
    return false;
}

static Token __last_token(const Parser* parser) {
    return parser->tokens->items[parser->pos - 1];
}

static bool __expect_and_bump(Parser* parser, TokenType type, Token* out) {
    if (!__bump(parser, out)) {
        return false;
    }
    if (out->type != type) {
        fprintf(stderr, "[ERROR]: Expected token: %d, got: %d\n", type, out->type);
        return false;
    }
    return true;
}

static bool __bump(Parser* parser, Token* out) {
    if (!__peek(parser, out)) {
        fprintf(stderr, "[ERROR]: Tried to bump empty lexer\n");
        return false;
    }
    parser->pos++;
    return true;
}

static bool __peek(const Parser* parser, Token* out) {
    if (__empty(parser)) {
        fprintf(stderr, "[ERROR]: Tried to peek empty lexer\n");
        return false;
    }
    *out = parser->tokens->items[parser->pos];
    return true;
}

static bool __empty(const Parser* parser) {
    return parser->pos >= parser->tokens->count;
}
