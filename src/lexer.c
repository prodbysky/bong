#include "lexer.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include "da.h"
#include "error.h"

static bool __number(Lexer* lexer, Token* out);
static bool __kw_or_id(Lexer* lexer, Token* out);
static KeywordType __to_kw(const char* pos, size_t len);
static bool __done(const Lexer* lexer);
static void __skip_ws(Lexer* lexer);
static char __bump(Lexer* lexer);
static char __peek(const Lexer* lexer);

void print_token(const Token* t) {
    switch (t->type) {
        case TT_NUMBER: {
            fprintf(stderr, "Number: %lu", t->number);
            break;
        }
        case TT_SEMI: {
            fprintf(stderr, "Semicolon");
            break;
        }
        case TT_COLON: {
            fprintf(stderr, "Colon");
            break;
        }
        case TT_ASSIGN: {
            fprintf(stderr, "Assign");
            break;
        }
        case TT_OPEN_CURLY: {
            fprintf(stderr, "Open curly");
            break;
        }
        case TT_CLOSE_CURLY: {
            fprintf(stderr, "Close curly");
            break;
        }
        case TT_OPERATOR: {
            switch (t->op) {
                case OT_PLUS: fprintf(stderr, "Operator `+`"); break;
                case OT_MINUS: fprintf(stderr, "Operator `-`"); break;
                case OT_STAR: fprintf(stderr, "Operator `*`"); break;
                case OT_SLASH: fprintf(stderr, "Operator `/`"); break;
                case OT_LT: fprintf(stderr, "Operator `<`"); break;
                case OT_MT: fprintf(stderr, "Operator `>`"); break;
            }
            break;
        }
        case TT_KEYWORD: {
            switch (t->kw) {
                case KT_RETURN: fprintf(stderr, "Keyword: return"); break;
                case KT_IF: fprintf(stderr, "Keyword: if"); break;
                case KT_WHILE: fprintf(stderr, "Keyword: while"); break;
                case KT_NO: assert(false && "Unreachable this keyword is never produced"); break;
            }
            break;
        }
        case TT_IDENT: {
            fprintf(stderr, "Identifier: "STR_FMT, STR_ARG(&t->id));
            break;
        }
    }
}

bool lexer_run(Lexer* lexer, Tokens* out) {
    while (__skip_ws(lexer), !__done(lexer)) {
        if (isdigit(__peek(lexer))) {
            Token t = {0};
            if (!__number(lexer, &t)) return false;
            da_push(out, t, lexer->arena);
            continue;
        }
        if (isalpha(__peek(lexer)) || __peek(lexer) == '_') {
            Token t = {0};
            if (!__kw_or_id(lexer, &t)) return false;
            da_push(out, t, lexer->arena);
            continue;
        }
        if (__peek(lexer) == '+') {
            Token t = {.type = TT_OPERATOR, .offset = lexer->pos, .op = OT_PLUS, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            __bump(lexer);
            continue;
        }
        if (__peek(lexer) == '-') {
            Token t = {.type = TT_OPERATOR, .offset = lexer->pos, .op = OT_MINUS, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            __bump(lexer);
            continue;
        }
        if (__peek(lexer) == '*') {
            Token t = {.type = TT_OPERATOR, .offset = lexer->pos, .op = OT_STAR, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            __bump(lexer);
            continue;
        }
        if (__peek(lexer) == '/') {
            Token t = {.type = TT_OPERATOR, .offset = lexer->pos, .op = OT_SLASH, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            __bump(lexer);
            continue;
        }
        if (__peek(lexer) == ';') {
            Token t = {.type = TT_SEMI, .offset = lexer->pos, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            __bump(lexer);
            continue;
        }
        if (__peek(lexer) == ':') {
            Token t = {.type = TT_COLON, .offset = lexer->pos, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            __bump(lexer);
            continue;
        }
        if (__peek(lexer) == '=') {
            Token t = {.type = TT_ASSIGN, .offset = lexer->pos, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            __bump(lexer);
            continue;
        }
        if (__peek(lexer) == '{') {
            Token t = {.type = TT_OPEN_CURLY, .offset = lexer->pos, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            __bump(lexer);
            continue;
        }
        if (__peek(lexer) == '}') {
            Token t = {.type = TT_CLOSE_CURLY, .offset = lexer->pos, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            __bump(lexer);
            continue;
        }
        if (__peek(lexer) == '<') {
            Token t = {.type = TT_OPERATOR, .op = OT_LT, .offset = lexer->pos, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            __bump(lexer);
            continue;
        }
        if (__peek(lexer) == '>') {
            Token t = {.type = TT_OPERATOR, .op = OT_MT, .offset = lexer->pos, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            __bump(lexer);
            continue;
        }
        fprintf(stderr, "[ERROR]: Unknown char found when lexing the source code: %c\n", __peek(lexer));
        bong_error(lexer->source, lexer->pos);
        return false;
    }

    return true;
}


static bool __number(Lexer* lexer, Token* out) {
    out->offset = lexer->pos;
    out->type = TT_NUMBER;
    out->file = lexer->source;
    while (!__done(lexer) && isdigit(__peek(lexer))) __bump(lexer);
    if (__done(lexer)) {
        out->number = strtoull(lexer->source->content.items + out->offset, NULL, 10);
        out->len = lexer->pos - out->offset;
        return true;
    }
    if (isalpha(__peek(lexer))) {
        fprintf(stderr, "[ERROR]: Non-separated number literal found\n");
        bong_error(lexer->source, lexer->pos);
        return false;
    }
    out->number = strtoull(lexer->source->content.items + out->offset, NULL, 10);
    out->len = lexer->pos - out->offset;
    return true;
}

static bool __kw_or_id(Lexer* lexer, Token* out) {
    out->offset = lexer->pos;
    out->file = lexer->source;
    while (!__done(lexer) && (isalnum(__peek(lexer)) || __peek(lexer) == '_')) __bump(lexer);
    out->kw = __to_kw(lexer->source->content.items + out->offset, lexer->pos - out->offset);
    if (!out->kw) {
        out->id.items = &lexer->source->content.items[out->offset];
        out->id.count = lexer->pos - out->offset;
        out->type = TT_IDENT;
        return true;
    } else {
        out->type = TT_KEYWORD;
        out->len = lexer->pos - out->offset;
        return true;
    }
}

static KeywordType __to_kw(const char* pos, size_t len) {
    if (len == 6 && strncmp(pos, "return", 6) == 0) return KT_RETURN;
    if (len == 5 && strncmp(pos, "while", 5) == 0) return KT_WHILE;
    if (len == 2 && strncmp(pos, "if", 2) == 0) return KT_IF;

    return KT_NO;
}

static void __skip_ws(Lexer* lexer) {
    while (!__done(lexer) && isspace(__peek(lexer))) __bump(lexer);
}

static char __bump(Lexer* lexer) {
    if (__done(lexer)) {
        fprintf(stderr, "[ERROR]: Tried to bump empty lexer\n");
        return 0;
    }
    return lexer->source->content.items[lexer->pos++];
}

static char __peek(const Lexer* lexer) {
    if (__done(lexer)) {
        fprintf(stderr, "[ERROR]: Tried to peek empty lexer\n");
        return 0;
    }
    return lexer->source->content.items[lexer->pos];
}

static bool __done(const Lexer* lexer) {
    return lexer->pos >= lexer->source->content.count;
}

