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
bool parser_peek(const Parser* parser, Token* out);
bool parser_bump(Parser* parser, Token* out);
bool parser_expect_and_bump(Parser* parser, TokenType type, Token* out);
bool parser_empty(const Parser* parser);

bool parser_stmt(Parser* parser, Stmt* out);
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
bool parser_expression(Parser* parser, Expr* out);
bool parser_eq(Parser* parser, Expr* out);
bool parser_cmp(Parser* parser, Expr* out);
bool parser_term(Parser* parser, Expr* out);
bool parser_factor(Parser* parser, Expr* out);
bool parser_unary(Parser* parser, Expr* out);
bool parser_primary(Parser* parser, Expr* out);
Token parser_last_token(const Parser* parser);


// ---- Errors (locations) ----
typedef struct {
    size_t line;
    size_t col;
} Location;

Location get_loc(const SourceFile* file, size_t offset);
ptrdiff_t get_line_begin(const SourceFile* file, size_t offset);
ptrdiff_t get_line_end(const SourceFile* file, size_t offset);
void bong_error(const SourceFile* source, size_t begin);

// ---- IR ----
// This contains all of the logic that should be treated as external (since I'll probably make this a separate library)
// for generating final executables or other target files
// NO internal logic of Bong should be here
// When moving it out the da_append macro will only be an internal thing for this library
typedef enum {
    SHRIMP_IT_ADD,
    SHRIMP_IT_SUB,
    SHRIMP_IT_MUL,
    SHRIMP_IT_DIV,
    SHRIMP_IT_ASSIGN,
    SHRIMP_IT_RETURN,
} Shrimp_InstrType;

typedef enum {
    SHRIMP_VK_CONST,
    SHRIMP_VK_TEMP,
} Shrimp_ValueKind;

typedef uint64_t Shrimp_Temp;

typedef struct {
    Shrimp_ValueKind kind;
    union {
        uint64_t c;
        Shrimp_Temp t;
    };
} Shrimp_Value;

typedef struct {
    Shrimp_InstrType t;
    union {
        Shrimp_Value ret;
        struct {
            Shrimp_Value v;
            Shrimp_Temp into;
        } assign;
        struct {
            Shrimp_Value l;
            Shrimp_Value r;
            Shrimp_Temp result;
        } binop;
    };
} Shrimp_Instr;

typedef struct {
    const char* name;
    size_t temp_count;
    // body
    size_t count;
    size_t capacity;
    Shrimp_Instr* items;
} Shrimp_Function;

typedef struct {
    size_t count;
    size_t capacity;
    Shrimp_Function* items;
    const char* name;
} Shrimp_Module;

typedef enum {
    SHRIMP_TARGET_X86_64_NASM_LINUX,
    SHRIMP_TARGET_COUNT
} Shrimp_Target;

typedef enum {
    SHRIMP_OUTPUT_ASM,
    SHRIMP_OUTPUT_OBJ,
    SHRIMP_OUTPUT_EXE,
} Shrimp_OutputKind;

typedef enum {
    SHRIMP_OPT_NONE       = 0,
    SHRIMP_OPT_CONST_FOLD = 1,
    /* TODO
    SHRIMP_OPT_DEAD_CODE  = 2,
    SHRIMP_OPT_INLINE     = 4,
    */
} Shrimp_OptFlags;

typedef struct {
    Shrimp_Target target;
    Shrimp_OutputKind output_kind;
    Shrimp_OptFlags opts;
    // TODO: bool emit_debug_info;
    const char* output_name;
} Shrimp_CompOptions;

// user facing code (generating the IR)
Shrimp_Module Shrimp_module_new(const char* name);
void Shrimp_module_cleanup(Shrimp_Module mod);
Shrimp_Function* Shrimp_module_new_function(Shrimp_Module* mod, const char* name);
void Shrimp_function_return(Shrimp_Function* func, Shrimp_Value value);
Shrimp_Value Shrimp_function_add(Shrimp_Function* func, Shrimp_Value l, Shrimp_Value r);
Shrimp_Value Shrimp_function_sub(Shrimp_Function* func, Shrimp_Value l, Shrimp_Value r);
Shrimp_Value Shrimp_function_mul(Shrimp_Function* func, Shrimp_Value l, Shrimp_Value r);
Shrimp_Value Shrimp_function_div(Shrimp_Function* func, Shrimp_Value l, Shrimp_Value r);
Shrimp_Value Shrimp_function_alloc_temp(Shrimp_Function* func);
void Shrimp_function_assign_temp(Shrimp_Function* func, Shrimp_Value target, Shrimp_Value value);
Shrimp_Value Shrimp_value_make_const(uint64_t num);

void Shrimp_module_dump(FILE* file, Shrimp_Module mod);
void Shrimp_value_dump(FILE* file, Shrimp_Value v);

bool Shrimp_module_verify(const Shrimp_Module* mod);
bool Shrimp_module_compile(Shrimp_Module* mod, Shrimp_CompOptions opts);
void Shrimp_module_optimize(Shrimp_Module* mod, Shrimp_CompOptions opts);
void Shrimp_module_const_fold(Shrimp_Module* mod);
bool Shrimp_module_x86_64_nasm_linux_compile(const Shrimp_Module* mod, Shrimp_CompOptions opts);

// codegen part ( TODO: add function to generate code according to the supported targets )
bool Shrimp_module_x86_64_dump_nasm_mod(const Shrimp_Module* mod, FILE* file);
void Shrimp_x86_64_nasm_mov_value_to_reg(const Shrimp_Value* value, const char* reg, FILE* out);

bool generate_mod(Body* nodes, Shrimp_Module* out);
Shrimp_Value generate_expr(const Expr* n, Shrimp_Function* out);


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
    Body nodes = {0};
    if (!parser_parse(&p, &nodes)) return 1;
    Shrimp_Module mod = Shrimp_module_new("main");
    if (!generate_mod(&nodes, &mod)) return false;
    Shrimp_CompOptions opts = {
        .target = SHRIMP_TARGET_X86_64_NASM_LINUX,
        .opts = SHRIMP_OPT_NONE,
        .output_kind = SHRIMP_OUTPUT_EXE,
        .output_name = mod.name
    };
    if (!Shrimp_module_compile(&mod, opts)) return false;
    Shrimp_module_dump(stdout, mod);
}

static void help(const char* prog_name) {
    fprintf(stderr, "%s [OPTIONS] <input.bg>\n", prog_name);
    fprintf(stderr, "OPTIONS:\n");
    fprintf(stderr, "  -help: Prints this help message");
}

bool generate_mod(Body* nodes, Shrimp_Module* out) {
    *out = Shrimp_module_new("main");
    Shrimp_Function* main_func = Shrimp_module_new_function(out, "_start");
    for (size_t i = 0; i < nodes->count; i++) {
        switch (nodes->items[i].type) {
            case ST_RET: {
                Shrimp_Value value = generate_expr(&nodes->items[i].ret, main_func);
                Shrimp_function_return(main_func, value);
                break;
            }
        }
    }
    if (!Shrimp_module_verify(out)) return false;
    return true;
}

Shrimp_Value generate_expr(const Expr* n, Shrimp_Function* out) {
    switch (n->type) {
        case ET_NUMBER: {
            return Shrimp_value_make_const(n->number);
        }
        case ET_BIN: {
            Shrimp_Value l = generate_expr(n->bin.l, out);
            Shrimp_Value r = generate_expr(n->bin.r, out);
            switch (n->bin.op) {
                case OT_PLUS: {
                    return Shrimp_function_add(out, l, r);
                }
                case OT_MINUS: {
                    return Shrimp_function_sub(out, l, r);
                }
                case OT_STAR: {
                    return Shrimp_function_mul(out, l, r);
                }
                case OT_SLASH: {
                    return Shrimp_function_div(out, l, r);
                }
            }
        }
    }
    assert(false);
}

#define SHRIMP_DA_INIT_CAP 16
#define Shrimp_da_push(arr, item) do { \
    if ((arr)->capacity == 0) {\
        (arr)->capacity = DA_INIT_CAP;\
        (arr)->items = malloc(sizeof(*(arr)->items) * (arr)->capacity);\
    }\
    if ((arr)->count >= (arr)->capacity) {\
        (arr)->capacity *= 1.5; \
        (arr)->items = realloc((arr)->items, (arr)->capacity); \
    }\
    (arr)->items[(arr)->count++] = (item);\
} while (false)

Shrimp_Module Shrimp_module_new(const char* name) {
    return (Shrimp_Module){.name = name};
}

bool Shrimp_module_compile(Shrimp_Module* mod, Shrimp_CompOptions opts) {
    if (!Shrimp_module_verify(mod)) return false;
    if (opts.opts) Shrimp_module_optimize(mod, opts);
    switch (opts.target) {
        case SHRIMP_TARGET_X86_64_NASM_LINUX: return Shrimp_module_x86_64_nasm_linux_compile(mod, opts);
        default: {
            fprintf(stderr, "[ERROR]: Unknown target %d\n", opts.target);
            return false;
        }
    }
    return true;
}

void Shrimp_module_optimize(Shrimp_Module* mod, Shrimp_CompOptions opts) {
    if (opts.opts & SHRIMP_OPT_CONST_FOLD) Shrimp_module_const_fold(mod);
}

typedef struct {
    Shrimp_Temp idx;
    uint64_t value;
} IndexValuePair;

typedef struct {
    IndexValuePair* items;
    size_t count;
    size_t capacity;
} IndexValuePairs;

IndexValuePair* find_pair(const IndexValuePairs* pairs, Shrimp_Value value) {
    if (value.kind == SHRIMP_VK_CONST) return NULL;
    for (size_t i = 0; i < pairs->count; i++) {
        if (pairs->items[i].idx == value.t) {
            return &pairs->items[i];
        }
    }
    return NULL;
}

void Shrimp_module_const_fold(Shrimp_Module* mod) {
    for (size_t f_i = 0; f_i < mod->count; f_i++) {
        Shrimp_Function* f = &mod->items[f_i];
        IndexValuePairs pairs = {0};
        for (size_t i_i = 0; i_i < f->count; i_i++) {
            Shrimp_Instr* instr = &f->items[i_i];
            switch (instr->t) {
                case SHRIMP_IT_ASSIGN: {
                    if (instr->assign.v.kind == SHRIMP_VK_CONST) {
                        IndexValuePair p = {
                            .idx = instr->assign.into,
                            .value = instr->assign.v.c,
                        };
                        Shrimp_da_push(&pairs, p);
                        break;
                    }
                    IndexValuePair* p = find_pair(&pairs, instr->assign.v);
                    if (p && instr->assign.v.kind != SHRIMP_VK_CONST) {
                        instr->assign.v = (Shrimp_Value) {
                            .kind = SHRIMP_VK_CONST,
                            .c = p->value
                        };
                    }
                    break;
                }
                case SHRIMP_IT_RETURN: {
                    IndexValuePair* p = find_pair(&pairs, instr->ret);
                    if (p && instr->ret.kind != SHRIMP_VK_CONST) {
                        instr->ret = (Shrimp_Value) {
                            .kind = SHRIMP_VK_CONST,
                            .c = p->value
                        };
                    }
                    break;
                }
                case SHRIMP_IT_ADD: {
                    bool l_const = instr->binop.l.kind == SHRIMP_VK_CONST;
                    bool r_const = instr->binop.r.kind == SHRIMP_VK_CONST;

                    IndexValuePair* l_pair = l_const ? NULL : find_pair(&pairs, instr->binop.l);
                    IndexValuePair* r_pair = r_const ? NULL : find_pair(&pairs, instr->binop.r);

                    const size_t* l_val = l_const ? &instr->binop.l.c : (l_pair ? &l_pair->value : NULL);
                    const size_t* r_val = r_const ? &instr->binop.r.c : (r_pair ? &r_pair->value : NULL);

                    if (l_val && r_val && instr->t != SHRIMP_IT_ASSIGN) {
                        *instr = (Shrimp_Instr) {
                            .t = SHRIMP_IT_ASSIGN,
                            .assign = {
                                .into = instr->binop.result,
                                .v = {
                                    .kind = SHRIMP_VK_CONST,
                                    .c = *l_val + *r_val
                                }
                            }
                        };
                        IndexValuePair p =  {
                            .value = *l_val + *r_val,
                            .idx = instr->assign.into
                        };
                        Shrimp_da_push(&pairs, p);
                    }
                    break;
                }
                case SHRIMP_IT_SUB: {
                    bool l_const = instr->binop.l.kind == SHRIMP_VK_CONST;
                    bool r_const = instr->binop.r.kind == SHRIMP_VK_CONST;

                    IndexValuePair* l_pair = l_const ? NULL : find_pair(&pairs, instr->binop.l);
                    IndexValuePair* r_pair = r_const ? NULL : find_pair(&pairs, instr->binop.r);

                    const size_t* l_val = l_const ? &instr->binop.l.c : (l_pair ? &l_pair->value : NULL);
                    const size_t* r_val = r_const ? &instr->binop.r.c : (r_pair ? &r_pair->value : NULL);

                    if (l_val && r_val && instr->t != SHRIMP_IT_ASSIGN) {
                        *instr = (Shrimp_Instr) {
                            .t = SHRIMP_IT_ASSIGN,
                            .assign = {
                                .into = instr->binop.result,
                                .v = {
                                    .kind = SHRIMP_VK_CONST,
                                    .c = *l_val - *r_val
                                }
                            }
                        };
                        IndexValuePair p =  {
                            .value = *l_val - *r_val,
                            .idx = instr->assign.into
                        };
                        Shrimp_da_push(&pairs, p);
                    }
                    break;
                }
                case SHRIMP_IT_MUL: {
                    bool l_const = instr->binop.l.kind == SHRIMP_VK_CONST;
                    bool r_const = instr->binop.r.kind == SHRIMP_VK_CONST;

                    IndexValuePair* l_pair = l_const ? NULL : find_pair(&pairs, instr->binop.l);
                    IndexValuePair* r_pair = r_const ? NULL : find_pair(&pairs, instr->binop.r);

                    const size_t* l_val = l_const ? &instr->binop.l.c : (l_pair ? &l_pair->value : NULL);
                    const size_t* r_val = r_const ? &instr->binop.r.c : (r_pair ? &r_pair->value : NULL);

                    if (l_val && r_val && instr->t != SHRIMP_IT_ASSIGN) {
                        *instr = (Shrimp_Instr) {
                            .t = SHRIMP_IT_ASSIGN,
                            .assign = {
                                .into = instr->binop.result,
                                .v = {
                                    .kind = SHRIMP_VK_CONST,
                                    .c = *l_val * *r_val
                                }
                            }
                        };
                        IndexValuePair p =  {
                            .value = *l_val - *r_val,
                            .idx = instr->assign.into
                        };
                        Shrimp_da_push(&pairs, p);
                    }
                    break;
                }
                case SHRIMP_IT_DIV: {
                    bool l_const = instr->binop.l.kind == SHRIMP_VK_CONST;
                    bool r_const = instr->binop.r.kind == SHRIMP_VK_CONST;

                    IndexValuePair* l_pair = l_const ? NULL : find_pair(&pairs, instr->binop.l);
                    IndexValuePair* r_pair = r_const ? NULL : find_pair(&pairs, instr->binop.r);

                    const size_t* l_val = l_const ? &instr->binop.l.c : (l_pair ? &l_pair->value : NULL);
                    const size_t* r_val = r_const ? &instr->binop.r.c : (r_pair ? &r_pair->value : NULL);

                    if (l_val && r_val && instr->t != SHRIMP_IT_ASSIGN) {
                        *instr = (Shrimp_Instr) {
                            .t = SHRIMP_IT_ASSIGN,
                            .assign = {
                                .into = instr->binop.result,
                                .v = {
                                    .kind = SHRIMP_VK_CONST,
                                    .c = *l_val / *r_val
                                }
                            }
                        };
                        IndexValuePair p =  {
                            .value = *l_val - *r_val,
                            .idx = instr->assign.into
                        };
                        Shrimp_da_push(&pairs, p);
                    }
                    break;
                }
            }
        }
    }
}

bool Shrimp_module_verify(const Shrimp_Module* mod) {
    for (size_t f_i = 0; f_i < mod->count; f_i++) {
        const Shrimp_Function* func = &mod->items[f_i];
        for (size_t i_i = 0; i_i < func->count; i_i++) {
            const Shrimp_Instr* ins = &func->items[i_i];
            switch (ins->t) {
                case SHRIMP_IT_RETURN: {
                    continue;
                }
                case SHRIMP_IT_ADD: case SHRIMP_IT_SUB: case SHRIMP_IT_MUL: {
                    if (ins->binop.result >= func->temp_count) {
                        fprintf(stderr, "[Shrimp: Module %s, function %s, verification failure]: Result of add instruction cannot be a non-temporary value\n", mod->name, func->name);
                        return false;
                    }
                    break;
                }
                case SHRIMP_IT_DIV: {
                    if (ins->binop.result >= func->temp_count) {
                        fprintf(stderr, "[Shrimp: Module %s, function %s, verification failure]: Result of add instruction cannot be a non-temporary value\n", mod->name, func->name);
                        return false;
                    }
                    break;
                    if (ins->binop.r.kind == SHRIMP_VK_CONST && ins->binop.r.c == 0) {
                        fprintf(stderr, "[Shrimp: Module %s, function %s, verification failure]: Attempted to divide by zero\n", mod->name, func->name);
                        return false;
                    }
                    break;
                }
                case SHRIMP_IT_ASSIGN: {
                    if (ins->assign.into >= func->temp_count) {
                        fprintf(stderr, "[Shrimp: Module %s, function %s, verification failure]: Place of assign instruction cannot be a non-temporary value\n", mod->name, func->name);
                        return false;
                    }
                    break;
                }
            }
        }
    }
    return true;
}

bool Shrimp_module_x86_64_nasm_linux_compile(const Shrimp_Module* mod, Shrimp_CompOptions opts) {
    char asm_path[256] = {0};
    char o_path[256] = {0};
    snprintf(asm_path, sizeof(asm_path), "%s.asm", opts.output_name);
    snprintf(o_path, sizeof(o_path), "%s.o", opts.output_name);

    FILE* asm_file = fopen(asm_path, "wb");

    if (!Shrimp_module_x86_64_dump_nasm_mod(mod, asm_file)) {
        fprintf(stderr, "[ERROR]: Failed to generate assembly\n");
        return false;
    }

    fclose(asm_file);

    if (opts.output_kind == SHRIMP_OUTPUT_ASM) {
        fprintf(stderr, "[INFO]: Generated %s\n", asm_path);
        return true;
    }

    // TODO: Don't use system here
    char command_buffer[1024] = {0};
    snprintf(command_buffer, sizeof(command_buffer), "nasm %s -felf64 -o %s", asm_path, o_path);
    system(command_buffer);

    if (opts.output_kind == SHRIMP_OUTPUT_OBJ) {
        fprintf(stderr, "[INFO]: Generated %s\n", o_path);
        return true;
    }
    memset(command_buffer, 0, sizeof(command_buffer));
    snprintf(command_buffer, sizeof(command_buffer), "ld %s -o %s", o_path, opts.output_name);
    system(command_buffer);
    fprintf(stderr, "[INFO]: Generated %s\n", opts.output_name);
    return true;
}

void Shrimp_module_dump(FILE* file, Shrimp_Module mod) {
    for (size_t i = 0; i < mod.count; i++) {
        const Shrimp_Function* func = &mod.items[i];
        fprintf(file, "func %s() {\n", func->name);
        for (size_t j = 0; j < func->count; j++) {
            const Shrimp_Instr* instr = &func->items[j];
            fprintf(file, "  ");
            switch (instr->t) {
                case SHRIMP_IT_ADD: {
                    fprintf(file, "$%zu <- ", instr->binop.result);
                    Shrimp_value_dump(file, instr->binop.l);
                    fprintf(file, " + ");
                    Shrimp_value_dump(file, instr->binop.r);
                    break;
                }
                case SHRIMP_IT_SUB: {
                    fprintf(file, "$%zu <- ", instr->binop.result);
                    Shrimp_value_dump(file, instr->binop.l);
                    fprintf(file, " - ");
                    Shrimp_value_dump(file, instr->binop.r);
                    break;
                }
                case SHRIMP_IT_MUL: {
                    fprintf(file, "$%zu <- ", instr->binop.result);
                    Shrimp_value_dump(file, instr->binop.l);
                    fprintf(file, " * ");
                    Shrimp_value_dump(file, instr->binop.r);
                    break;
                }
                case SHRIMP_IT_DIV: {
                    fprintf(file, "$%zu <- ", instr->binop.result);
                    Shrimp_value_dump(file, instr->binop.l);
                    fprintf(file, " / ");
                    Shrimp_value_dump(file, instr->binop.r);
                    break;
                }
                case SHRIMP_IT_ASSIGN: {
                    fprintf(file, "$%zu <- ", instr->assign.into);
                    Shrimp_value_dump(file, instr->assign.v);
                    break;
                }
                case SHRIMP_IT_RETURN: {
                    fprintf(file, "return ");
                    Shrimp_value_dump(file, instr->ret);
                    break;
                }
            }
            fprintf(file, "\n");
        }
        fprintf(file, "}\n");
    }
}

void Shrimp_value_dump(FILE* file, Shrimp_Value v) {
    switch (v.kind) {
        case SHRIMP_VK_CONST: {
            fprintf(file, "%zu", v.c);
            break;
        }
        case SHRIMP_VK_TEMP: {
            fprintf(file, "$%zu", v.t);
            break;
        }
    }
}

void Shrimp_module_cleanup(Shrimp_Module mod) {
    if (mod.items != NULL) free(mod.items);
}

Shrimp_Function* Shrimp_module_new_function(Shrimp_Module* mod, const char* name) {
    Shrimp_Function f = {.name = name};
    Shrimp_da_push(mod, f);
    return &mod->items[mod->count-1];
}

void Shrimp_function_return(Shrimp_Function* func, Shrimp_Value value) {
    Shrimp_Instr instr = {
        .t = SHRIMP_IT_RETURN,
        .ret = value
    };
    Shrimp_da_push(func, instr);
}
Shrimp_Value Shrimp_function_add(Shrimp_Function* func, Shrimp_Value l, Shrimp_Value r) {
    Shrimp_Value result = Shrimp_function_alloc_temp(func);
    Shrimp_Instr instr = {
        .t = SHRIMP_IT_ADD,
        .binop = {
            .l = l,
            .r = r,
            .result = result.t
        }
    };
    Shrimp_da_push(func, instr);

    return result;
}

Shrimp_Value Shrimp_function_sub(Shrimp_Function* func, Shrimp_Value l, Shrimp_Value r) {
    Shrimp_Value result = Shrimp_function_alloc_temp(func);
    Shrimp_Instr instr = {
        .t = SHRIMP_IT_SUB,
        .binop = {
            .l = l,
            .r = r,
            .result = result.t
        }
    };
    Shrimp_da_push(func, instr);

    return result;
}

Shrimp_Value Shrimp_function_mul(Shrimp_Function* func, Shrimp_Value l, Shrimp_Value r) {
    Shrimp_Value result = Shrimp_function_alloc_temp(func);
    Shrimp_Instr instr = {
        .t = SHRIMP_IT_MUL,
        .binop = {
            .l = l,
            .r = r,
            .result = result.t
        }
    };
    Shrimp_da_push(func, instr);

    return result;
}
Shrimp_Value Shrimp_function_div(Shrimp_Function* func, Shrimp_Value l, Shrimp_Value r) {
    Shrimp_Value result = Shrimp_function_alloc_temp(func);
    Shrimp_Instr instr = {
        .t = SHRIMP_IT_DIV,
        .binop = {
            .l = l,
            .r = r,
            .result = result.t
        }
    };
    Shrimp_da_push(func, instr);

    return result;
}

Shrimp_Value Shrimp_function_alloc_temp(Shrimp_Function* func) {
    return (Shrimp_Value) {
        .kind = SHRIMP_VK_TEMP,
        .t = func->temp_count++
    };
}

Shrimp_Value Shrimp_value_make_const(uint64_t num) {
    return (Shrimp_Value){
        .kind = SHRIMP_VK_CONST,
        .c = num
    };
}
void Shrimp_function_assign_temp(Shrimp_Function* func, Shrimp_Value target, Shrimp_Value value) {
    assert(target.kind == SHRIMP_VK_TEMP);
    Shrimp_Instr instr = {
        .t = SHRIMP_IT_ASSIGN,
        .assign = {
            .v = value,
            .into = target.t
        }
    };
    Shrimp_da_push(func, instr);
}

bool Shrimp_module_x86_64_dump_nasm_mod(const Shrimp_Module* mod, FILE* file) {
    for (size_t i = 0; i < mod->count; i++) {
        fprintf(file, "section .text\n");
        fprintf(file, "global _start\n");
        const Shrimp_Function* f = &mod->items[i];
        fprintf(file, "%s:\n", f->name);
        fprintf(file, "  push rbp\n");
        fprintf(file, "  mov rbp, rsp\n");
        fprintf(file, "  sub rsp, %zu\n", f->temp_count * 8);

        // store callee saved registers according to the x86_64 sysV AMD64 abi
        fprintf(file, "  push rbx\n");
        fprintf(file, "  push r12\n");
        fprintf(file, "  push r13\n");
        fprintf(file, "  push r14\n");
        fprintf(file, "  push r15\n");

        for (size_t j = 0; j < f->count; j++) {
            const Shrimp_Instr* instr = &f->items[j];
            switch (instr->t) {
                case SHRIMP_IT_ADD: {
                    fprintf(file, "  mov qword [rbp - %zu], 0\n", (instr->binop.result + 1) * 8);
                    Shrimp_x86_64_nasm_mov_value_to_reg(&instr->binop.l, "r10", file);
                    Shrimp_x86_64_nasm_mov_value_to_reg(&instr->binop.r, "r11", file);
                    fprintf(file, "  add [rbp - %zu], r10\n", (instr->binop.result + 1) * 8);
                    fprintf(file, "  add [rbp - %zu], r11\n", (instr->binop.result + 1) * 8);
                    break;
                }
                case SHRIMP_IT_SUB: {
                    fprintf(file, "  mov qword [rbp - %zu], 0\n", (instr->binop.result + 1) * 8);
                    Shrimp_x86_64_nasm_mov_value_to_reg(&instr->binop.l, "r10", file);
                    Shrimp_x86_64_nasm_mov_value_to_reg(&instr->binop.r, "r11", file);
                    fprintf(file, "  sub [rbp - %zu], r10\n", (instr->binop.result + 1) * 8);
                    fprintf(file, "  sub [rbp - %zu], r11\n", (instr->binop.result + 1) * 8);
                    break;
                }
                case SHRIMP_IT_MUL: {
                    Shrimp_x86_64_nasm_mov_value_to_reg(&instr->binop.l, "r10", file);
                    Shrimp_x86_64_nasm_mov_value_to_reg(&instr->binop.r, "r11", file);
                    fprintf(file, "  imul r10, r11\n");
                    fprintf(file, "  mov qword [rbp - %zu], r10\n", (instr->binop.result + 1) * 8);
                    break;
                }
                case SHRIMP_IT_DIV: {
                    fprintf(file, "  mov rdx, 0\n");
                    Shrimp_x86_64_nasm_mov_value_to_reg(&instr->binop.l, "rax", file);
                    Shrimp_x86_64_nasm_mov_value_to_reg(&instr->binop.r, "r10", file);
                    fprintf(file, "  idiv r10\n");
                    fprintf(file, "  mov [rbp - %zu], rax\n", (instr->binop.result + 1) * 8);
                    break;
                }
                case SHRIMP_IT_ASSIGN: {
                    Shrimp_x86_64_nasm_mov_value_to_reg(&instr->assign.v, "r10", file);
                    fprintf(file, "  mov [rbp - %zu], r10\n", (instr->assign.into + 1) * 8);
                    break;
                }
                case SHRIMP_IT_RETURN: {
                    Shrimp_x86_64_nasm_mov_value_to_reg(&instr->ret, "rax", file);
                    fprintf(file, "  jmp .exit\n");
                    break;
                }
            }
        }
        fprintf(file, "  .exit:\n");

        fprintf(file, "  pop r15\n");
        fprintf(file, "  pop r14\n");
        fprintf(file, "  pop r13\n");
        fprintf(file, "  pop r12\n");
        fprintf(file, "  pop rbx\n");

        fprintf(file, "  mov rsp, rbp\n");
        fprintf(file, "  pop rbp\n");
        fprintf(file, "  mov rdi, rax\n");
        fprintf(file, "  mov rax, 60\n");
        fprintf(file, "  syscall\n");
    }
    return true;
}

void Shrimp_x86_64_nasm_mov_value_to_reg(const Shrimp_Value* value, const char* reg, FILE* out) {
    fprintf(out, "  mov %s, ", reg);
    switch(value->kind) {
        case SHRIMP_VK_TEMP: {
            fprintf(out, "[rbp - %zu]\n", (value->t + 1) * 8);
            break;
        }
        case SHRIMP_VK_CONST: {
            fprintf(out, "%zu\n", value->c);
            break;
        }
    }
}

bool parse_config(int argc, char** argv, Config* out) {
    out->prog_name = *argv++; argc--;
    if (argc == 0) {
        fprintf(stderr, "[ERROR]: No flags/inputs/subcommands provided\n");
        help(out->prog_name);
        exit(0);
    }
    while (argc != 0) {
        if (strcmp(*argv, "-help") == 0) {
            help(out->prog_name);
            exit(0);
        } else {
            if (**argv == '-') {
                fprintf(stderr, "[ERROR]: Not known flag supplied\n");
                help(out->prog_name);
                return false;
            } else if (out->input != NULL) {
                fprintf(stderr, "[ERROR]: Multiple input files provided\n");
                help(out->prog_name);
                return false;
            } else {
                out->input = *argv++; argc--;
            }
        }
    }
    return true;
}

bool parser_parse(Parser* parser, Body* out) {
    while (!parser_empty(parser)) {
        Stmt n = {0};
        if (!parser_stmt(parser, &n)) return false;
        da_push(out, n, parser->arena);
    }
    return true;
}

bool parser_stmt(Parser* parser, Stmt* out) {
    Token curr;
    if (!parser_bump(parser, &curr)) {
        fprintf(stderr, "[ERROR]: Missing keyword for statment\n");
        bong_error(parser->source, parser_last_token(parser).offset);
        return false;
    }
    if (curr.type != TT_KEYWORD) {
        fprintf(stderr, "[ERROR]: No statement (for now) starts with a token that is not a keyword\n");
        bong_error(parser->source, parser_last_token(parser).offset);
        return false;
    }
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
    assert(false);
}
bool parser_expression(Parser* parser, Expr* out) {
    return parser_eq(parser, out);
}
bool parser_eq(Parser* parser, Expr* out) {
    return parser_cmp(parser, out);
}
bool parser_cmp(Parser* parser, Expr* out) {
    return parser_term(parser, out);
}
bool parser_term(Parser* parser, Expr* out) {
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
bool parser_factor(Parser* parser, Expr* out) {
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
bool parser_unary(Parser* parser, Expr* out) {
    return parser_primary(parser, out);
}
bool parser_primary(Parser* parser, Expr* out) {
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
        default: {
            fprintf(stderr, "[ERROR]: Unexpected token in place of primary expression %d\n", t.type);
            bong_error(parser->source, t.offset);
            return false;
        }
    }
    return false;
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
        fprintf(stderr, "[ERROR]: Expected token: %d, got: %d\n", type, out->type);
        bong_error(parser->source, out->offset);
        return false;
    }
    return true;
}
bool parser_bump(Parser* parser, Token* out) {
    if (!parser_peek(parser, out)) {
        fprintf(stderr, "[ERROR]: Tried to bump empty lexer\n");
        return false;
    }
    parser->pos++;
    return true;
}
bool parser_peek(const Parser* parser, Token* out) {
    if (parser_empty(parser)) {
        fprintf(stderr, "[ERROR]: Tried to peek empty lexer\n");
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
            fprintf(stderr, "Number: %lu", t->number);
            break;
        }
        case TT_SEMI: {
            fprintf(stderr, "Semicolon");
            break;
        }
        case TT_OPERATOR: {
            switch (t->op) {
                case OT_PLUS: fprintf(stderr, "Operator `+`"); break;
                case OT_MINUS: fprintf(stderr, "Operator `-`"); break;
                case OT_STAR: fprintf(stderr, "Operator `*`"); break;
                case OT_SLASH: fprintf(stderr, "Operator `/`"); break;
            }
            break;
        }
        case TT_KEYWORD: {
            switch (t->kw) {
                case KT_RETURN: fprintf(stderr, "Keyword: return"); break;
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
        fprintf(stderr, "[ERROR]: Unknown char found when lexing the source code: %c\n", lexer_peek(lexer));
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
        fprintf(stderr, "[ERROR]: Non-separated number literal found\n");
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
        fprintf(stderr, "[ERROR]: No custom identifiers are supported\n");
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
    for (size_t i = file->content.count - 1; i > offset && i > 0; i--) {
        if (file->content.items[i] == '\n') return i;
    }
#ifdef DEBUG
    fprintf(stderr, "[DEBUG]: Tried to get invalid end of line offset in %s with offset %zu\n", file->name, offset);
#endif
    return -1;
}

void bong_error(const SourceFile* source, size_t begin) {
    Location loc = get_loc(source, begin);
    fprintf(stderr, "./%s:%zu:%zu\n", source->name, loc.line, loc.col);
    ptrdiff_t l_begin = get_line_begin(source, begin);
    fprintf(stderr, "%.*s\n", (int)(get_line_end(source, begin) - l_begin), &source->content.items[l_begin]);
}

void lexer_skip_ws(Lexer* lexer) {
    while (!lexer_done(lexer) && isspace(lexer_peek(lexer))) lexer_bump(lexer);
}

char lexer_bump(Lexer* lexer) {
    if (lexer_done(lexer)) {
        fprintf(stderr, "[ERROR]: Tried to bump empty lexer\n");
        return 0;
    }
    return lexer->source->content.items[lexer->pos++];
}
char lexer_peek(const Lexer* lexer) {
    if (lexer_done(lexer)) {
        fprintf(stderr, "[ERROR]: Tried to peek empty lexer\n");
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
    fprintf(stderr, "[DEBUG]: Allocated %zu bytes when %zu is available\n", size, a->capacity - a->used);
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
    fprintf(stderr, "[DEBUG]: Read file %s (size: %zu)\n", path, size);
#endif
    fclose(file);
    return true;
}
