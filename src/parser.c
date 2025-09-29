#include "parser.h"
#include "da.h"
#include <stdio.h>
#include "error.h"
#include "lexer.h"
#include <assert.h>

static bool parser_peek(const Parser* parser, Token* out);
static bool parser_bump(Parser* parser, Token* out);
static bool parser_expect_and_bump(Parser* parser, TokenType type, Token* out);
static bool parser_empty(const Parser* parser);

static bool parser_stmt(Parser* parser, Stmt* out);
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
static bool parser_expression(Parser* parser, Expr* out);
static bool parser_eq(Parser* parser, Expr* out);
static bool parser_cmp(Parser* parser, Expr* out);
static bool parser_term(Parser* parser, Expr* out);
static bool parser_factor(Parser* parser, Expr* out);
static bool parser_unary(Parser* parser, Expr* out);
static bool parser_primary(Parser* parser, Expr* out);
static Token parser_last_token(const Parser* parser);

bool parser_parse(Parser* parser, Body* out) {
    while (!parser_empty(parser)) {
        Stmt n = {0};
        if (!parser_stmt(parser, &n)) return false;
        da_push(out, n, parser->arena);
    }
    return true;
}

static bool parser_stmt(Parser* parser, Stmt* out) {
    Token curr;
    if (!parser_bump(parser, &curr)) {
        fprintf(stderr, "[ERROR]: Missing keyword for statment\n");
        bong_error(parser->source, parser_last_token(parser).offset);
        return false;
    }

    switch (curr.type) {
        case TT_KEYWORD: {
            switch (curr.kw) {
                case KT_NO: assert(false);
                case KT_RETURN: {
                    Expr e;
                    if (!parser_expression(parser, &e)) return false;
                    out->type = ST_RET;
                    out->ret = e;
                    Token hopefully_semi = {0};
                    if (!parser_expect_and_bump(parser, TT_SEMI, &hopefully_semi)) {
                        fprintf(stderr, "[ERROR]: Missing statment termination semicolon\n");
                        bong_error(parser->source, hopefully_semi.offset);
                        return false;
                    }
                    return true;
                }
            }
        }
        case TT_IDENT: {
            Token name = curr;
            if (!parser_bump(parser, &curr)) return false;
            switch (curr.type) {
                case TT_COLON: {
                    if (!parser_expect_and_bump(parser, TT_ASSIGN, &curr)) {
                        fprintf(stderr, "[ERROR]: After a bare identifier a [colon]-assign token is expected\n");
                        bong_error(parser->source, parser_last_token(parser).offset);
                        return false;
                    }
                    Expr e = {0};
                    if (!parser_expression(parser, &e)) return false;
                    if (!parser_expect_and_bump(parser, TT_SEMI, &curr)) {
                        fprintf(stderr, "[ERROR]: A semicolon is expected after the expression of the var define statement\n");
                        bong_error(parser->source, parser_last_token(parser).offset);
                        return false;
                    }
                    out->type = ST_VAR_DEF;
                    out->var_def.name = name.id;
                    out->var_def.value = e;
                    return true;
                }
                case TT_ASSIGN: {
                    Expr e = {0};
                    if (!parser_expression(parser, &e)) return false;
                    out->type = ST_VAR_REASSIGN;
                    out->var_reassign.name = name.id;
                    out->var_reassign.value = e;
                    if (!parser_expect_and_bump(parser, TT_SEMI, &curr)) {
                        fprintf(stderr, "[ERROR]: A semicolon is expected after the expression of the var define statement\n");
                        bong_error(parser->source, parser_last_token(parser).offset);
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
static bool parser_expression(Parser* parser, Expr* out) {
    return parser_eq(parser, out);
}

static bool parser_eq(Parser* parser, Expr* out) {
    return parser_cmp(parser, out);
}

static bool parser_cmp(Parser* parser, Expr* out) {
    return parser_term(parser, out);
}

static bool parser_term(Parser* parser, Expr* out) {
    if (!parser_factor(parser, out)) return false;
    Token t = {0};
    while (!parser_empty(parser) && parser_peek(parser, &t) && (t.type == TT_OPERATOR && (t.op == OT_PLUS || t.op == OT_MINUS))) {
        parser_bump(parser, &t);
        Expr* left = arena_alloc(parser->arena, sizeof(Expr));
        *left = *out;
        out->type = ET_BIN;
        out->bin.l = left;
        out->bin.op = t.op;
        out->bin.r = arena_alloc(parser->arena, sizeof(Expr));
        if (!parser_factor(parser, out->bin.r)) return false;
    }
    return true;
}

static bool parser_factor(Parser* parser, Expr* out) {
    if (!parser_unary(parser, out)) return false;
    Token t = {0};
    while (!parser_empty(parser) && parser_peek(parser, &t) && (t.type == TT_OPERATOR && (t.op == OT_STAR || t.op == OT_SLASH))) {
        parser_bump(parser, &t);
        Expr* left = arena_alloc(parser->arena, sizeof(Expr));
        *left = *out;
        out->type = ET_BIN;
        out->bin.l = left;
        out->bin.op = t.op;
        out->bin.r = arena_alloc(parser->arena, sizeof(Expr));
        if (!parser_unary(parser, out->bin.r)) return false;
    }
    return true;
}
static bool parser_unary(Parser* parser, Expr* out) {
    return parser_primary(parser, out);
}
static bool parser_primary(Parser* parser, Expr* out) {
    Token t = {0};
    if (!parser_bump(parser, &t)) {
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

static Token parser_last_token(const Parser* parser) {
    return parser->tokens->items[parser->pos - 1];
}

static bool parser_expect_and_bump(Parser* parser, TokenType type, Token* out) {
    if (!parser_bump(parser, out)) {
        return false;
    }
    if (out->type != type) {
        // TODO: Human readable token printing
        fprintf(stderr, "[ERROR]: Expected token: %d, got: %d\n", type, out->type);
        bong_error(parser->source, out->offset);
        return false;
    }
    return true;
}

static bool parser_bump(Parser* parser, Token* out) {
    if (!parser_peek(parser, out)) {
        fprintf(stderr, "[ERROR]: Tried to bump empty lexer\n");
        return false;
    }
    parser->pos++;
    return true;
}

static bool parser_peek(const Parser* parser, Token* out) {
    if (parser_empty(parser)) {
        fprintf(stderr, "[ERROR]: Tried to peek empty lexer\n");
        return false;
    }
    *out = parser->tokens->items[parser->pos];
    return true;
}

static bool parser_empty(const Parser* parser) {
    return parser->pos >= parser->tokens->count;
}
