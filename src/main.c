#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NOB_IMPLEMENTATION
#include "../nob.h"

// ---- Dynamic arrays ----
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

// ---- Arena allocator ----
typedef struct {
    uint8_t* buffer;
    size_t capacity;
    size_t used;
} Arena;

Arena arena_new(size_t size);
void* arena_alloc(Arena* a, size_t size);

// ---- Easier strings
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

// ---- File system ----
typedef struct {
    String content;
    const char* name;
} SourceFile;

bool read_entire_file(const char* path, SourceFile* f, Arena* arena);

// ---- Config from CLI
typedef struct {
    const char* prog_name;
    const char* input;
} Config;

bool parse_config(int argc, char** argv, Config* out);

// ---- Lexer ----
typedef enum {
    TT_NUMBER,
    TT_OPERATOR,
    TT_SEMI,
    TT_KEYWORD,
} TokenType;

typedef enum {
    OT_PLUS,
    OT_MINUS,
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

bool lexer_done(const Lexer* lexer);
void lexer_skip_ws(Lexer* lexer);
char lexer_bump(Lexer* lexer);
char lexer_peek(const Lexer* lexer);
bool lexer_run(Lexer* lexer, Tokens* out);
bool lexer_number(Lexer* lexer, Token* out);
bool lexer_kw_or_id(Lexer* lexer, Token* out);
KeywordType lexer_to_kw(const char* pos, size_t len);
void print_token(const Token* t);

// ---- Parser ----
typedef struct {
    SourceFile const* source;
    Tokens* tokens;
    Arena* arena;
    size_t pos;
} Parser;

typedef enum {
    NT_NUMBER,
    NT_BIN,
    // first statement like expression, which results in the last expression in a block
    // OR an explicit return <value>;
    // also all of these subexpressions are of primary binding power so they are the same as numbers, ids, ...
    NT_RET,
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
        struct Node* ret;
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
Token parser_last_token(const Parser* parser);
void print_node(const Node* n, int indent);


// ---- Errors (locations) ----
typedef struct {
    size_t line;
    size_t col;
} Location;

Location get_loc(const SourceFile* file, size_t offset);
ptrdiff_t get_line_begin(const SourceFile* file, size_t offset);
ptrdiff_t get_line_end(const SourceFile* file, size_t offset);
void bong_error(const SourceFile* source, size_t begin);


int main(int argc, char** argv) {
    Arena arena = arena_new(1024 * 1024 * 8);
    Config c = {0};
    if (!parse_config(argc, argv, &c)) return false;
    SourceFile file = {0};
    if (!read_entire_file(c.input, &file, &arena)) return 1;
    Lexer l = {
        .pos = 0,
        .source = &file,
        .arena = &arena
    };
    Tokens tokens = {0};
    if (!lexer_run(&l, &tokens)) return 1;
    Parser p = {
        .arena = &arena,
        .pos = 0,
        .source = &file,
        .tokens = &tokens,
    };
    Nodes nodes = {0};
    if (!parser_parse(&p, &nodes)) return 1;
}

static void help(const char* prog_name) {
    printf("%s [OPTIONS] <input.bg>\n", prog_name);
    printf("OPTIONS:\n");
    printf("  -help: Prints this help message");
}

bool parse_config(int argc, char** argv, Config* out) {
    out->prog_name = *argv++; argc--;
    if (argc == 0) {
        printf("[ERROR]: No flags/inputs/subcommands provided\n");
        help(out->prog_name);
        exit(0);
    }
    while (argc != 0) {
        if (strcmp(*argv, "-help") == 0) {
            help(out->prog_name);
            exit(0);
        } else {
            if (**argv == '-') {
                printf("[ERROR]: Not known flag supplied\n");
                help(out->prog_name);
                return false;
            } else if (out->input != NULL) {
                printf("[ERROR]: Multiple input files provided\n");
                help(out->prog_name);
                return false;
            } else {
                out->input = *argv++; argc--;
            }
        }
    }
    return true;
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
        case NT_RET: {
            printf("Return: \n");
            print_node(n->ret, indent + 1);
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
        parser_peek(parser, &t);
        if (t.type != TT_OPERATOR && (t.op != OT_PLUS || t.op != OT_MINUS)) {
            break;
        }
        parser_bump(parser, &t);
        Node* left = arena_alloc(parser->arena, sizeof(Node));
        *left = *out; // copy old expression into left
        out->type = NT_BIN;
        out->bin.l = left;
        out->bin.op = t.op;
        out->bin.r = arena_alloc(parser->arena, sizeof(Node));
        if (!parser_factor(parser, out->bin.r)) return false;
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
        case TT_KEYWORD: {
            switch (t.kw) {
                case KT_NO: assert(false); break;
                case KT_RETURN: {
                    Token dummy;
                    out->type = NT_RET;
                    out->ret = NULL;

                    out->ret = arena_alloc(parser->arena, sizeof(Node));
                    if (!parser_expression(parser, out->ret)) return false;

                    if (!parser_bump(parser, &dummy)) return false;

                    return true;
                }
            }
        }
        default: {
            printf("[ERROR]: Unexpected token in place of primary expression %d\n", t.type);
            bong_error(parser->source, t.offset);
            return false;
        }
    }
}

Token parser_last_token(const Parser* parser) {
    return parser->tokens->items[parser->pos - 1];
}

bool parser_expect_and_bump(Parser* parser, TokenType type, Token* out) {
    if (!parser_bump(parser, out)) {
        return false;
    }
    if (out->type != type) {
        // TODO: Human readable token printing
        printf("[ERROR]: Expected token: %d, got: %d\n", type, out->type);
        bong_error(parser->source, out->offset);
        return false;
    }
    return true;
}
bool parser_bump(Parser* parser, Token* out) {
    if (!parser_peek(parser, out)) {
        printf("[ERROR]: Tried to bump empty lexer\n");
        return false;
    }
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
        case TT_SEMI: {
            printf("Semicolon");
            break;
        }
        case TT_OPERATOR: {
            switch (t->op) {
                case OT_PLUS: printf("Operator `+`"); break;
                case OT_MINUS: printf("Operator `-`"); break;
            }
            break;
        }
        case TT_KEYWORD: {
            switch (t->kw) {
                case KT_RETURN: printf("Keyword: return"); break;
                case KT_NO: assert(false && "Unreachable this keyword is never produced"); break;
            }
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
        if (isalpha(lexer_peek(lexer))) {
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
        if (lexer_peek(lexer) == ';') {
            Token t = {.type = TT_SEMI, .offset = lexer->pos, .len = 1, .file = lexer->source};
            da_push(out, t, lexer->arena);
            lexer_bump(lexer);
            continue;
        }
        printf("[ERROR]: Unknown char found when lexing the source code: %c\n", lexer_peek(lexer));
        bong_error(lexer->source, lexer->pos);
        return false;
    }

    return true;
}


bool lexer_number(Lexer* lexer, Token* out) {
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
        printf("[ERROR]: Non-separated number literal found\n");
        bong_error(lexer->source, lexer->pos);
        return false;
    }
    out->number = strtoull(lexer->source->content.items + out->offset, NULL, 10);
    out->len = lexer->pos - out->offset;
    return true;
}

bool lexer_kw_or_id(Lexer* lexer, Token* out) {
    out->offset = lexer->pos;
    out->file = lexer->source;
    while (!lexer_done(lexer) && isalnum(lexer_peek(lexer))) lexer_bump(lexer);
    out->kw = lexer_to_kw(lexer->source->content.items + out->offset, lexer->pos - out->offset);
    if (!out->kw) {
        printf("[ERROR]: No custom identifiers are supported\n");
        return false;
    }
    out->type = TT_KEYWORD;
    out->len = lexer->pos - out->offset;
    return true;
}

KeywordType lexer_to_kw(const char* pos, size_t len) {
    if (len == 6 && strncmp(pos, "return", 6) == 0) return KT_RETURN;

    return KT_NO;
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
    for (int i = file->content.count - 1; i > offset && i > 0; i--) {
        if (file->content.items[i] == '\n') return i;
    }
#ifdef DEBUG
    printf("[DEBUG]: Tried to get invalid end of line offset in %s with offset %zu\n", file->name, offset);
#endif
    return -1;
}

void bong_error(const SourceFile* source, size_t begin) {
    Location loc = get_loc(source, begin);
    printf("./%s:%zu:%zu\n", source->name, loc.line, loc.col);
    ptrdiff_t l_begin = get_line_begin(source, begin);
    printf("%.*s\n", (int)(get_line_end(source, begin) - l_begin), &source->content.items[l_begin]);
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
#ifdef DEBUG
    printf("[DEBUG]: Allocated %zu bytes when %zu is available\n", size, a->capacity - a->used);
#endif
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
#ifdef DEBUG
    printf("[DEBUG]: Read file %s (size: %zu)\n", path, size);
#endif
    fclose(file);
    return true;
}
