#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stddef.h>
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

typedef struct {
    String content;
    const char* name;
} SourceFile;

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
    SourceFile const * file;
    size_t offset;
    size_t len;
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
    SourceFile const* source;
    Arena* arena;
    size_t pos;
} Lexer;

typedef struct {
    size_t line;
    size_t col;
} Location;

typedef struct {
    SourceFile const* source;
    Tokens* tokens;
    Token last_token;
    Arena* arena;
    size_t pos;
} Parser;

typedef enum {
    NT_NUMBER,
    NT_BIN
} NodeType;

typedef struct Node {
    NodeType type;
    union {
        uint64_t number;
        struct {
            struct Node* l;
            struct Node* r;
            OperatorType op;
        } bin;
    };
} Node;

typedef struct {
    Node* items;
    size_t count;
    size_t capacity;
} Nodes;

bool parser_parse(Parser* parser, Nodes* out);
bool parser_peek(const Parser* parser, Token* out);
bool parser_bump(Parser* parser, Token* out);
bool parser_expect_and_bump(Parser* parser, TokenType type, Token* out);
bool parser_empty(const Parser* parser);
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
bool parser_expression(Parser* parser, Node* out);
bool parser_eq(Parser* parser, Node* out);
bool parser_cmp(Parser* parser, Node* out);
bool parser_term(Parser* parser, Node* out);
bool parser_factor(Parser* parser, Node* out);
bool parser_unary(Parser* parser, Node* out);
bool parser_primary(Parser* parser, Node* out);
void print_node(const Node* n, int indent);

bool lexer_done(const Lexer* lexer);
void lexer_skip_ws(Lexer* lexer);
char lexer_bump(Lexer* lexer);
char lexer_peek(const Lexer* lexer);
bool lexer_run(Lexer* lexer, Tokens* out);
bool lexer_number(Lexer* lexer, Token* out);
void print_token(const Token* t);

// All of these can probably be mushed together
Location get_loc(const SourceFile* file, size_t offset);
ptrdiff_t get_line_begin(const SourceFile* file, size_t offset);
ptrdiff_t get_line_end(const SourceFile* file, size_t offset);

bool read_entire_file(const char* path, SourceFile* f, Arena* arena);

Arena arena_new(size_t size);
void* arena_alloc(Arena* a, size_t size);

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
    Parser p = {
        .arena = &arena,
        .last_token = tokens.items[0],
        .pos = 0,
        .source = &file,
        .tokens = &tokens,
    };
    Nodes nodes = {0};
    parser_parse(&p, &nodes);
    print_node(&nodes.items[0], 0);
}

void print_node(const Node* n, int indent) {
    switch (n->type) {
        case NT_NUMBER: {
            printf("%.*s%zu\n", indent * 2, " ", n->number);
            return;
        }
        case NT_BIN: {
            print_node(n->bin.l, indent + 1);
            print_node(n->bin.r, indent + 1);
            return;
        }
    }
}

bool parser_parse(Parser* parser, Nodes* out) {
    while (!parser_empty(parser)) {
        Node n = {0};
        if (!parser_expression(parser, &n)) return false;
        da_push(out, n, parser->arena);
    }
    return true;
}
bool parser_expression(Parser* parser, Node* out) {
    return parser_eq(parser, out);
}
bool parser_eq(Parser* parser, Node* out) {
    return parser_cmp(parser, out);
}
bool parser_cmp(Parser* parser, Node* out) {
    return parser_term(parser, out);
}
bool parser_term(Parser* parser, Node* out) {
    if (!parser_factor(parser, out)) return false;
    while (!parser_empty(parser)) {
        Token t = {0};
        if (!parser_expect_and_bump(parser, TT_OPERATOR, &t)) {
            break;
        }
        if (t.op == OT_PLUS || t.op == OT_MINUS) {
            Node* left = arena_alloc(parser->arena, sizeof(Node));
            *left = *out; // copy old expression into left
            out->type = NT_BIN;
            out->bin.l = left;
            out->bin.op = t.op;
            out->bin.r = arena_alloc(parser->arena, sizeof(Node));
            if (!parser_factor(parser, out->bin.r)) return false;
        }
    }
    return true;
}
bool parser_factor(Parser* parser, Node* out) {
    return parser_unary(parser, out);
}
bool parser_unary(Parser* parser, Node* out) {
    return parser_primary(parser, out);
}
bool parser_primary(Parser* parser, Node* out) {
    Token t = {0};
    if (!parser_bump(parser, &t)) {
        printf("[ERROR]: Missing expression\n");
        return false;
    }
    switch (t.type) {
        case TT_NUMBER: {
            out->type = NT_NUMBER;
            out->number = t.number;
            return true;
        }
        default: {
            printf("[ERROR]: Unexpected token in place of primary expression %d\n", t.type);
            return false;
        }
    }
}

bool parser_expect_and_bump(Parser* parser, TokenType type, Token* out) {
    if (!parser_bump(parser, out)) {
        return false;
    }
    if (out->type != type) {
        // TODO: Human readable token printing
        printf("[ERROR]: Expected token: %d, got: %d\n", type, out->type);
        return false;
    }
    return true;
}
bool parser_bump(Parser* parser, Token* out) {
    if (!parser_peek(parser, out)) {
        printf("[ERROR]: Tried to bump empty lexer\n");
        return false;
    }
    parser->last_token = *out;
    parser->pos++;
    return true;
}
bool parser_peek(const Parser* parser, Token* out) {
    if (parser_empty(parser)) {
        printf("[ERROR]: Tried to peek empty lexer\n");
        return false;
    }
    *out = parser->tokens->items[parser->pos];
    return true;
}

bool parser_empty(const Parser* parser) {
    return parser->pos >= parser->tokens->count;
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
        // We loooove lumps of code
        // I don't want to over abstract this code so when I see the pattern I'll do so
        printf("[ERROR]: Unknown char found when lexing the source code: %c\n", lexer_peek(lexer));
        Location loc = get_loc(lexer->source, lexer->pos);
        printf("./%s:%zu:%zu\n", lexer->source->name, loc.line, loc.col);
        ptrdiff_t begin = get_line_begin(lexer->source, lexer->pos);
        printf("%.*s\n", (int)(get_line_end(lexer->source, lexer->pos) - begin), &lexer->source->content.items[begin]);
        return false;
    }

    return true;
}

bool lexer_number(Lexer* lexer, Token* out) {
    out->offset = lexer->pos;
    out->type = TT_NUMBER;
    out->file = lexer->source;
    while (!lexer_done(lexer) && isalnum(lexer_peek(lexer))) lexer_bump(lexer);
    if (lexer_done(lexer)) {
        out->number = strtoull(lexer->source->content.items + out->offset, NULL, 10);
        out->len = lexer->pos - out->offset;
        return true;
    }
    if (isalpha(lexer_peek(lexer))) {
        printf("[ERROR]: Non-separated number literal found\n");
        Location loc = get_loc(lexer->source, lexer->pos);
        printf("./%s:%zu:%zu\n", lexer->source->name, loc.line, loc.col);
        ptrdiff_t begin = get_line_begin(lexer->source, lexer->pos);
        printf("%.*s\n", (int)(get_line_end(lexer->source, lexer->pos) - begin), &lexer->source->content.items[begin]);
        return false;
    }
    out->number = strtoull(lexer->source->content.items + out->offset, NULL, 10);
    out->len = lexer->pos - out->offset;
    return true;
}
Location get_loc(const SourceFile* file, size_t offset) {
    Location l = {.line = 1, .col = 1};
    for (size_t i = 0; i < offset && i < file->content.count; i++) {
        if (file->content.items[i] == '\n') {
            l.line += 1;
            l.col = 1;
        } else {
            l.col++;
        }
    }
    return l;
}

ptrdiff_t get_line_begin(const SourceFile* file, size_t offset) {
    ptrdiff_t result = 0;
    for (size_t i = 0; i < offset && i < file->content.count; i++) {
        if (file->content.items[i] == '\n') result = i;
    }
    return result;
}

ptrdiff_t get_line_end(const SourceFile* file, size_t offset) {
    ptrdiff_t result = 0;
    for (int i = file->content.count - 1; i > offset && i > 0; i--) {
        if (file->content.items[i] == '\n') return i;
    }
    return -1;
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
