#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NOB_IMPLEMENTATION
#include "../nob.h"


#define DA_INIT_CAP 16
#define da_push(arr, item, arena) do { \
    if ((arr)->capacity == 0) {\
        (arr)->capacity = DA_INIT_CAP;\
        (arr)->items = arena_alloc((arena), sizeof(*(arr)->items) * (arr)->capacity);\
    }\
    if ((arr)->count >= (arr)->capacity) {\
        (arr)->capacity *= 1.5; \
        void* old = (arr)->items; \
        (arr)->items = arena_alloc((arena), sizeof(*(arr)->items) * (arr)->capacity);\
        memcpy((arr)->items, old, sizeof(*(arr)->items) * ((arr)->capacity / 1.5)); \
    }\
    (arr)->items[(arr)->count++] = (item);\
} while (false)

typedef struct {
    uint8_t* buffer;
    size_t capacity;
    size_t used;
} Arena;

typedef struct {
    char* items;
    size_t count;
    size_t capacity;
} String;

typedef struct {
    char const* items;
    size_t count;
} StringView;

#define STR_FMT "%.*s" 
#define STR_ARG(s) (int)(s)->count, (s)->items

typedef enum {
    TT_NUMBER,
    TT_OPERATOR
} TokenType;

typedef enum {
    OT_PLUS,
    OT_MINUS,
} OperatorType;

typedef struct {
    TokenType type;
    size_t offset;
    union {
        uint64_t number;
        OperatorType op;
    };
} Token;

typedef struct {
    Token* items;
    size_t count;
    size_t capacity;
} Tokens;


typedef struct {
    String content;
    const char* name;
} SourceFile;

typedef struct {
    SourceFile const* source;
    Arena* arena;
    size_t pos;
} Lexer;


bool lexer_done(const Lexer* lexer);
void lexer_skip_ws(Lexer* lexer);
char lexer_bump(Lexer* lexer);
char lexer_peek(const Lexer* lexer);
bool lexer_run(Lexer* lexer, Tokens* out);
bool lexer_number(Lexer* lexer, Token* out);

void print_token(const Token* t);

Arena arena_new(size_t size);
void* arena_alloc(Arena* a, size_t size);

bool read_entire_file(const char* path, SourceFile* f, Arena* arena);

int main(int argc, char** argv) {
    Arena arena = arena_new(1024 * 1024 * 8);
    SourceFile file = {0};
    if (!read_entire_file("test.bg", &file, &arena)) return 1;
    Lexer l = {
        .pos = 0,
        .source = &file,
        .arena = &arena
    };
    Tokens tokens = {0};
    if (!lexer_run(&l, &tokens)) return 1;
    for (size_t i = 0; i < tokens.count; i++) {
        print_token(&tokens.items[i]);
        printf("\n");
    }
}

void print_token(const Token* t) {
    switch (t->type) {
        case TT_NUMBER: {
            printf("Number: %lu", t->number);
            break;
        }
        case TT_OPERATOR: {
            switch (t->op) {
                case OT_PLUS: printf("Operator `+`"); break;
                case OT_MINUS: printf("Operator `-`"); break;
            }
            break;
        }
    }
}

bool lexer_run(Lexer* lexer, Tokens* out) {
    while (lexer_skip_ws(lexer), !lexer_done(lexer)) {
        if (isalnum(lexer_peek(lexer))) {
            Token t = {0};
            if (!lexer_number(lexer, &t)) return false;
            da_push(out, t, lexer->arena);
            continue;
        }
        if (lexer_peek(lexer) == '+') {
            Token t = {.type = TT_OPERATOR, .offset = lexer->pos, .op = OT_PLUS};
            da_push(out, t, lexer->arena);
            lexer_bump(lexer);
            continue;
        }
        if (lexer_peek(lexer) == '-') {
            Token t = {.type = TT_OPERATOR, .offset = lexer->pos, .op = OT_MINUS};
            da_push(out, t, lexer->arena);
            lexer_bump(lexer);
            continue;
        }
    }

    return true;
}

bool lexer_number(Lexer* lexer, Token* out) {
    out->offset = lexer->pos;
    out->type = TT_NUMBER;
    while (!lexer_done(lexer) && isalnum(lexer_peek(lexer))) lexer_bump(lexer);
    if (lexer_done(lexer)) {
        out->number = strtoull(lexer->source->content.items + out->offset, NULL, 10);
        return true;
    }
    if (isalpha(lexer_peek(lexer))) {
        printf("[ERROR]: Non-separated number literal found\n");
        return false;
    }
    out->number = strtoull(lexer->source->content.items + out->offset, NULL, 10);
    return true;
}

void lexer_skip_ws(Lexer* lexer) {
    while (!lexer_done(lexer) && isspace(lexer_peek(lexer))) lexer_bump(lexer);
}

char lexer_bump(Lexer* lexer) {
    if (lexer_done(lexer)) {
        printf("[ERROR]: Tried to bump empty lexer\n");
        return 0;
    }
    return lexer->source->content.items[lexer->pos++];
}
char lexer_peek(const Lexer* lexer) {
    if (lexer_done(lexer)) {
        printf("[ERROR]: Tried to peek empty lexer\n");
        return 0;
    }
    return lexer->source->content.items[lexer->pos];
}

bool lexer_done(const Lexer* lexer) {
    return lexer->pos >= lexer->source->content.count;
}

Arena arena_new(size_t size) {
    Arena a = {0};
    a.capacity = size;
    a.buffer = calloc(size, sizeof(uint8_t));
    assert(a.buffer && "Ran out of RAM?");
    return a;
}

void* arena_alloc(Arena* a, size_t size) {
    assert(a->buffer);
    assert(a->used + size < a->capacity);
    void* buf = a->buffer + a->used;
    a->used += size;
    return buf;
}

void arena_free(Arena* a) {
    assert(a->buffer);
    free(a->buffer);
    memset(a, 0, sizeof(Arena));
}

bool read_entire_file(const char* path, SourceFile* f, Arena* arena) {
    assert(path);
    f->name = path;
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "[ERROR]: Failed to read file %s: %s\n", path, strerror(errno));
        return false;
    }
    if (fseek(file, 0, SEEK_END) == -1) {
        fclose(file);
        fprintf(stderr, "[ERROR]: Failed to go to the end of file %s: %s\n", path, strerror(errno));
        return false;
    }
    size_t size = ftell(file);
    if (size == -1) {
        fclose(file);
        fprintf(stderr, "[ERROR]: Failed to get the size of file %s: %s\n", path, strerror(errno));
        return false;
    }
    if (fseek(file, 0, SEEK_SET) == -1) {
        fclose(file);
        fprintf(stderr, "[ERROR]: Failed to go back to the start of file %s: %s\n", path, strerror(errno));
        return false;
    }
    f->content.items = arena_alloc(arena, sizeof(char) * size);
    fread(f->content.items, sizeof(char), size, file);
    f->content.capacity = size;
    f->content.count = size;
    fclose(file);
    return true;
}
