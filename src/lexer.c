#include "lexer.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include "da.h"
#include "error.h"

static bool lexer_number(Lexer* lexer, Token* out);
static bool lexer_kw_or_id(Lexer* lexer, Token* out);
static KeywordType lexer_to_kw(const char* pos, size_t len);
static bool lexer_done(const Lexer* lexer);
static void lexer_skip_ws(Lexer* lexer);
static char lexer_bump(Lexer* lexer);
static char lexer_peek(const Lexer* lexer);


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
    while (lexer_skip_ws(lexer), !lexer_done(lexer)) {
        if (isdigit(lexer_peek(lexer))) {
            Token t = {0};
            if (!lexer_number(lexer, &t)) return false;
            da_push(out, t, lexer->arena);
            continue;
        }
        if (isalpha(lexer_peek(lexer)) || lexer_peek(lexer) == '_') {
            Token t = {0};
            if (!lexer_kw_or_id(lexer, &t)) return false;
            da_push(out, t, lexer->arena);
            continue;
        }
        if (lexer_peek(lexer) == '+') {
            Token t = {.type = TT_OPERATOR, .offset = lexer->pos, .op = OT_PLUS, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            lexer_bump(lexer);
            continue;
        }
        if (lexer_peek(lexer) == '-') {
            Token t = {.type = TT_OPERATOR, .offset = lexer->pos, .op = OT_MINUS, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            lexer_bump(lexer);
            continue;
        }
        if (lexer_peek(lexer) == '*') {
            Token t = {.type = TT_OPERATOR, .offset = lexer->pos, .op = OT_STAR, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            lexer_bump(lexer);
            continue;
        }
        if (lexer_peek(lexer) == '/') {
            Token t = {.type = TT_OPERATOR, .offset = lexer->pos, .op = OT_SLASH, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            lexer_bump(lexer);
            continue;
        }
        if (lexer_peek(lexer) == ';') {
            Token t = {.type = TT_SEMI, .offset = lexer->pos, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            lexer_bump(lexer);
            continue;
        }
        if (lexer_peek(lexer) == ':') {
            Token t = {.type = TT_COLON, .offset = lexer->pos, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            lexer_bump(lexer);
            continue;
        }
        if (lexer_peek(lexer) == '=') {
            Token t = {.type = TT_ASSIGN, .offset = lexer->pos, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            lexer_bump(lexer);
            continue;
        }
        if (lexer_peek(lexer) == '{') {
            Token t = {.type = TT_OPEN_CURLY, .offset = lexer->pos, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            lexer_bump(lexer);
            continue;
        }
        if (lexer_peek(lexer) == '}') {
            Token t = {.type = TT_CLOSE_CURLY, .offset = lexer->pos, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            lexer_bump(lexer);
            continue;
        }
        if (lexer_peek(lexer) == '<') {
            Token t = {.type = TT_OPERATOR, .op = OT_LT, .offset = lexer->pos, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            lexer_bump(lexer);
            continue;
        }
        if (lexer_peek(lexer) == '>') {
            Token t = {.type = TT_OPERATOR, .op = OT_MT, .offset = lexer->pos, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            lexer_bump(lexer);
            continue;
        }
        fprintf(stderr, "[ERROR]: Unknown char found when lexing the source code: %c\n", lexer_peek(lexer));
        bong_error(lexer->source, lexer->pos);
        return false;
    }

    return true;
}


static bool lexer_number(Lexer* lexer, Token* out) {
    out->offset = lexer->pos;
    out->type = TT_NUMBER;
    out->file = lexer->source;
    while (!lexer_done(lexer) && isdigit(lexer_peek(lexer))) lexer_bump(lexer);
    if (lexer_done(lexer)) {
        out->number = strtoull(lexer->source->content.items + out->offset, NULL, 10);
        out->len = lexer->pos - out->offset;
        return true;
    }
    if (isalpha(lexer_peek(lexer))) {
        fprintf(stderr, "[ERROR]: Non-separated number literal found\n");
        bong_error(lexer->source, lexer->pos);
        return false;
    }
    out->number = strtoull(lexer->source->content.items + out->offset, NULL, 10);
    out->len = lexer->pos - out->offset;
    return true;
}

static bool lexer_kw_or_id(Lexer* lexer, Token* out) {
    out->offset = lexer->pos;
    out->file = lexer->source;
    while (!lexer_done(lexer) && (isalnum(lexer_peek(lexer)) || lexer_peek(lexer) == '_')) lexer_bump(lexer);
    out->kw = lexer_to_kw(lexer->source->content.items + out->offset, lexer->pos - out->offset);
    if (!out->kw) {
        out->id.items = &lexer->source->content.items[out->offset];
        out->id.count = out->offset - lexer->pos;
        out->type = TT_IDENT;
        return true;
    } else {
        out->type = TT_KEYWORD;
        out->len = lexer->pos - out->offset;
        return true;
    }
}

static KeywordType lexer_to_kw(const char* pos, size_t len) {
    if (len == 6 && strncmp(pos, "return", 6) == 0) return KT_RETURN;
    if (len == 5 && strncmp(pos, "while", 5) == 0) return KT_WHILE;
    if (len == 2 && strncmp(pos, "if", 2) == 0) return KT_IF;

    return KT_NO;
}

static void lexer_skip_ws(Lexer* lexer) {
    while (!lexer_done(lexer) && isspace(lexer_peek(lexer))) lexer_bump(lexer);
}

static char lexer_bump(Lexer* lexer) {
    if (lexer_done(lexer)) {
        fprintf(stderr, "[ERROR]: Tried to bump empty lexer\n");
        return 0;
    }
    return lexer->source->content.items[lexer->pos++];
}

static char lexer_peek(const Lexer* lexer) {
    if (lexer_done(lexer)) {
        fprintf(stderr, "[ERROR]: Tried to peek empty lexer\n");
        return 0;
    }
    return lexer->source->content.items[lexer->pos];
}

static bool lexer_done(const Lexer* lexer) {
    return lexer->pos >= lexer->source->content.count;
}

