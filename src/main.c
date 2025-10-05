#include "str.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NOB_IMPLEMENTATION
#include "../nob.h"

#include "da.h"
#include "arena.h"
#include "fs.h"
#include "config.h"
#include "lexer.h"
#include "parser.h"

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
    SHRIMP_IT_JUMP,
    SHRIMP_IT_LABEL,
    SHRIMP_IT_CMP_LT,
    SHRIMP_IT_ASSIGN,
    SHRIMP_IT_RETURN,
    SHRIMP_IT_JUMP_IF_NOT,
} Shrimp_InstrType;

typedef enum {
    SHRIMP_VK_CONST,
    SHRIMP_VK_TEMP,
} Shrimp_ValueKind;

typedef uint64_t Shrimp_Temp;
typedef uint64_t Shrimp_Label;

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
        struct {
            Shrimp_Value cond;
            Shrimp_Label to;
        } jmp_if_not;
        struct {
            Shrimp_Label to;
        } jmp;
        Shrimp_Label label;
    };
} Shrimp_Instr;

typedef struct {
    const char* name;
    size_t temp_count;
    Shrimp_Label label_count;
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
Shrimp_Label Shrimp_function_label_alloc(Shrimp_Function* func);
void Shrimp_function_label_push(Shrimp_Function* func, Shrimp_Label label);
void Shrimp_function_return(Shrimp_Function* func, Shrimp_Value value);
Shrimp_Value Shrimp_function_add(Shrimp_Function* func, Shrimp_Value l, Shrimp_Value r);
Shrimp_Value Shrimp_function_sub(Shrimp_Function* func, Shrimp_Value l, Shrimp_Value r);
Shrimp_Value Shrimp_function_mul(Shrimp_Function* func, Shrimp_Value l, Shrimp_Value r);
Shrimp_Value Shrimp_function_div(Shrimp_Function* func, Shrimp_Value l, Shrimp_Value r);
Shrimp_Value Shrimp_function_cmp_lt(Shrimp_Function* func, Shrimp_Value l, Shrimp_Value r);
void Shrimp_function_jump_if_not(Shrimp_Function* func, Shrimp_Value v, Shrimp_Label l);
void Shrimp_function_jump(Shrimp_Function* func, Shrimp_Label l);
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


// our internal shrimp usage
typedef struct {
    StringView name;
    Shrimp_Value val;
} NameIRValue;

typedef struct {
    NameIRValue* items;
    size_t count;
    size_t capacity;
} VariableLUT;

void variableLUT_insert(VariableLUT* lut, StringView name, Shrimp_Value value, Arena* arena);
NameIRValue* variableLUT_get(const VariableLUT* lut, StringView name);

bool generate_mod(Body* nodes, Shrimp_Module* out, Arena* arena);
bool generate_statement(const Stmt* st, Shrimp_Function* out, VariableLUT* lut, Arena* arena);
bool generate_expr(Shrimp_Value* out_value, const Expr* n, Shrimp_Function* out, const VariableLUT* lut);


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
    if (!generate_mod(&nodes, &mod, &arena)) return false;
    Shrimp_CompOptions opts = {
        .target = SHRIMP_TARGET_X86_64_NASM_LINUX,
        .opts = SHRIMP_OPT_CONST_FOLD,
        .output_kind = SHRIMP_OUTPUT_EXE,
        .output_name = mod.name
    };
    if (!Shrimp_module_compile(&mod, opts)) return false;
    Shrimp_module_dump(stdout, mod);
}


bool generate_mod(Body* nodes, Shrimp_Module* out, Arena* arena) {
    *out = Shrimp_module_new("main");
    Shrimp_Function* main_func = Shrimp_module_new_function(out, "_start");
    VariableLUT lut = {0};
    for (size_t i = 0; i < nodes->count; i++) generate_statement(&nodes->items[i], main_func, &lut, arena);
    if (!Shrimp_module_verify(out)) return false;
    return true;
}

bool generate_statement(const Stmt* st, Shrimp_Function* out, VariableLUT* lut, Arena* arena) {
    switch(st->type) {
        case ST_RET: {
            Shrimp_Value value;
            if (!generate_expr(&value, &st->ret, out, lut)) return false;
            Shrimp_function_return(out, value);
            break;
        }
        case ST_VAR_DEF: {
            NameIRValue var = {
                .name = st->var_def.name,
            };
            if (!generate_expr(&var.val, &st->var_def.value, out, lut)) return false;
            da_push(lut, var, arena);
            break;
        }
        case ST_VAR_REASSIGN: {
            NameIRValue* var = variableLUT_get(lut, st->var_reassign.name);
            Shrimp_Value new = {0};
            if (!generate_expr(&new, &st->var_reassign.value, out, lut)) return false;
            Shrimp_function_assign_temp(out, var->val, new);
            break;
        }
        case ST_IF: {
            Shrimp_Label after = Shrimp_function_label_alloc(out);
            Shrimp_Value value;
            if (!generate_expr(&value, &st->if_st.cond, out, lut)) return false;
            Shrimp_function_jump_if_not(out, value, after);
            for (size_t j = 0; j < st->if_st.body.count; j++) generate_statement(&st->if_st.body.items[j], out, lut, arena);
            Shrimp_function_label_push(out, after);
            break;
        }
        case ST_WHILE: {
            Shrimp_Label condition = Shrimp_function_label_alloc(out);
            Shrimp_Label after = Shrimp_function_label_alloc(out);

            Shrimp_function_label_push(out, condition);
            Shrimp_Value value;
            if (!generate_expr(&value, &st->while_st.cond, out, lut)) return false;
            Shrimp_function_jump_if_not(out, value, after);

            for (size_t j = 0; j < st->while_st.body.count; j++) generate_statement(&st->while_st.body.items[j], out, lut, arena);

            Shrimp_function_jump(out, condition);
            Shrimp_function_label_push(out, after);
            break;
        }
    }
    return true;
}

bool generate_expr(Shrimp_Value* out_value, const Expr* n, Shrimp_Function* out, const VariableLUT* lut) {
    switch (n->type) {
        case ET_NUMBER: {
            *out_value = Shrimp_function_alloc_temp(out);
            Shrimp_function_assign_temp(out, *out_value, Shrimp_value_make_const(n->number));
            return true;
        }
        case ET_ID: {
            NameIRValue* var = variableLUT_get(lut, n->id);
            if (var == NULL) {
                fprintf(stderr, "[ERROR]: Unknown variable name used\n");
                return false;
            }
            *out_value = var->val;
            return true;
        }
        case ET_BIN: {
            Shrimp_Value l, r;
            if (!generate_expr(&l, n->bin.l, out, lut)) return false;
            if (!generate_expr(&r, n->bin.r, out, lut)) return false;
            switch (n->bin.op) {
                case OT_PLUS: {
                    *out_value = Shrimp_function_add(out, l, r);
                    return true;
                }
                case OT_MINUS: {
                    *out_value = Shrimp_function_sub(out, l, r);
                    return true;
                }
                case OT_STAR: {
                    *out_value = Shrimp_function_mul(out, l, r);
                    return true;
                }
                case OT_SLASH: {
                    *out_value = Shrimp_function_div(out, l, r);
                    return true;
                }
                case OT_LT: {
                    *out_value = Shrimp_function_cmp_lt(out, l, r);
                    return true;
                }
            }
        }
    }
    assert(false);
}

void variableLUT_insert(VariableLUT* lut, StringView name, Shrimp_Value value, Arena* arena) {
    NameIRValue n = {
        .name = name,
        .val = value
    };
    da_push(lut, n, arena);
}
NameIRValue* variableLUT_get(const VariableLUT* lut, StringView name) {
    for (size_t i = 0; i < lut->count; i++) {
        if (name.count == lut->items[i].name.count && strncmp(name.items, lut->items[i].name.items, name.count)) {
            return &lut->items[i];
        }
    }
    return NULL;
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
                case SHRIMP_IT_CMP_LT: {
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
                                    .c = *l_val < *r_val
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
                case SHRIMP_IT_LABEL: {
                    pairs.count = 0;
                    break;
                }
                case SHRIMP_IT_JUMP: case SHRIMP_IT_JUMP_IF_NOT: break;
                                                                                       
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
                case SHRIMP_IT_RETURN: case SHRIMP_IT_CMP_LT: {
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
                case SHRIMP_IT_LABEL: {
                    if (ins->label >= func->label_count) {
                        fprintf(stderr, "[Shrimp: Module %s, function %s, verification failure]: Invalid label inserted %zu when max is %zu\n", mod->name, func->name, ins->label, func->label_count);
                        return false;
                    }
                    break;
                }
                case SHRIMP_IT_JUMP: {
                    if (ins->jmp.to >= func->label_count) {
                        fprintf(stderr, "[Shrimp: Module %s, function %s, verification failure]: Invalid label provided to jump to %zu when max is %zu\n", mod->name, func->name, ins->label, func->label_count);
                        return false;
                    }
                    break;
                }
                case SHRIMP_IT_JUMP_IF_NOT: {
                    if (ins->jmp_if_not.to >= func->label_count) {
                        fprintf(stderr, "[Shrimp: Module %s, function %s, verification failure]: Invalid label provided to jump to %zu when max is %zu\n", mod->name, func->name, ins->label, func->label_count);
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
                case SHRIMP_IT_CMP_LT: {
                    fprintf(file, "$%zu <- ", instr->binop.result);
                    Shrimp_value_dump(file, instr->binop.l);
                    fprintf(file, " < ");
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
                case SHRIMP_IT_LABEL: {
                    fprintf(file, "%zu:", instr->label);
                    break;
                }
                case SHRIMP_IT_JUMP: {
                    fprintf(file, "jump @%zu", instr->jmp.to);
                    break;
                }
                case SHRIMP_IT_JUMP_IF_NOT: {
                    fprintf(file, "jump_z ");
                    Shrimp_value_dump(file, instr->jmp_if_not.cond);
                    fprintf(file, " @%zu", instr->jmp_if_not.to);
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

Shrimp_Label Shrimp_function_label_alloc(Shrimp_Function* func) {
    return func->label_count++;
}
void Shrimp_function_label_push(Shrimp_Function* func, Shrimp_Label label) {
    Shrimp_Instr lab = {
        .t = SHRIMP_IT_LABEL,
        .label = label
    };
    Shrimp_da_push(func, lab);
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

Shrimp_Value Shrimp_function_cmp_lt(Shrimp_Function* func, Shrimp_Value l, Shrimp_Value r) {
    Shrimp_Value result = Shrimp_function_alloc_temp(func);
    Shrimp_Instr instr = {
        .t = SHRIMP_IT_CMP_LT,
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

void Shrimp_function_jump_if_not(Shrimp_Function* func, Shrimp_Value v, Shrimp_Label l) {
    Shrimp_Instr instr = {
        .t = SHRIMP_IT_JUMP_IF_NOT,
        .jmp_if_not = {
            .cond = v,
            .to = l
        }
    };
    Shrimp_da_push(func, instr);
}
void Shrimp_function_jump(Shrimp_Function* func, Shrimp_Label l) {
    Shrimp_Instr instr = {
        .t = SHRIMP_IT_JUMP,
        .jmp = {
            .to = l
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
                    Shrimp_x86_64_nasm_mov_value_to_reg(&instr->binop.l, "r10", file);
                    fprintf(file, "  mov qword [rbp - %zu], r10\n", (instr->binop.result + 1) * 8);
                    Shrimp_x86_64_nasm_mov_value_to_reg(&instr->binop.r, "r11", file);
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
                case SHRIMP_IT_LABEL: {
                    fprintf(file, "  .%zu:\n", instr->label);
                    break;
                }
                case SHRIMP_IT_JUMP: {
                    fprintf(file, "  jmp .%zu\n", instr->jmp.to);
                    break;
                }
                case SHRIMP_IT_JUMP_IF_NOT: {
                    Shrimp_x86_64_nasm_mov_value_to_reg(&instr->jmp_if_not.cond, "r10", file);
                    fprintf(file, "  cmp r10, 0\n");
                    fprintf(file, "  jz .%zu\n", instr->jmp_if_not.to);
                    break;
                }
                case SHRIMP_IT_CMP_LT: {
                    Shrimp_x86_64_nasm_mov_value_to_reg(&instr->binop.l, "r10", file);
                    Shrimp_x86_64_nasm_mov_value_to_reg(&instr->binop.r, "r11", file);
                    fprintf(file, "  cmp r10, r11\n");
                    fprintf(file, "  mov r10, 0\n");
                    fprintf(file, "  mov r11, 1\n");
                    fprintf(file, "  cmovl r10, r11\n");
                    fprintf(file, "  mov qword [rbp - %zu], r10\n", (instr->binop.result + 1) * 8);
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
