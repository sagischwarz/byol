#include "../lib/mpc.h"
#include "../include/lval.h"
#include "../include/lenv.h"
#include "../include/io.h"

#ifdef _WIN32
#include <string.h>

static char buffer[2048];

char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';
    return cpy;
}

void add_history(char* unused) {}

#else
#include <editline/history.h>
#endif

#define LASSERT(args, cond, fmt, ...) \
    if (!(cond)) { \
        lval* err = lval_err(fmt, ##__VA_ARGS__); \
        lval_del(args); \
        return err; \
    }

#define LASSERT_ONEARG(args, func, expected) \
    LASSERT(args, args->count == expected, \
            "Function '%s' passed too many arguments! Got %i, expected %i.", \
            func, args->count, expected)

#define LASSERT_ELIST(args, func, i) \
    LASSERT(args, a->cell[i]->count != 0, \
            "Function '%s' passed {} as argument at index %i!", \
            func, i)

#define LASSERT_TYPE(args, func, i, expected) \
    LASSERT(args, args->cell[i]->type == expected,\
        "Function '%s' passed incorrect type at index %i! Got %s, expected %s.", \
        func, i, ltype_name(args->cell[i]->type), ltype_name(expected))

lval* lval_eval_sexpr(lenv* e, lval* v);
lval* lenv_get(lenv* e, lval* k);

lval* lval_eval(lenv* e, lval* v) {
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }

    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(e, v);
    }

    return v;
}

int is_int(double val) {
    return ceilf(val) == val;
}

char* ltype_name(int t) {
    switch(t) {
        case LVAL_FUN: return "Function";
        case LVAL_NUM: return "Number";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

lval* builtin_op(lenv* e, lval* a, char* op) {
    for (int i=0; i < a->count; i++) {
        LASSERT_TYPE(a, "op", i, LVAL_NUM)
    }

    lval* x = lval_pop(a, 0);

    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    while (a->count > 0) {
        lval* y = lval_pop(a, 0);

        if (strcmp(op, "+") == 0) {x->num += y->num;}
        if (strcmp(op, "-") == 0) {x->num -= y->num;}
        if (strcmp(op, "*") == 0) {x->num *= y->num;}
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x); lval_del(y);
                x = lval_err("Division by zero"); break;
            }
            x->num /= y->num;
        }
        if (strcmp(op, "%") == 0) {
            if (is_int(x->num) && is_int(y->num)) {
                x->num = (double)((int) x->num % (int) y->num);
            } else {
                return lval_err("Modulo works only on integers, got x=%f and y=%f.", x->num, y->num);
            }
        }
        lval_del(y);
    }
    lval_del(a); return(x);
}

lval* builtin_add(lenv* e, lval* a) {
    return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
    return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
    return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
    return builtin_op(e, a, "/");
}

lval* builtin_mod(lenv* e, lval* a) {
    return builtin_op(e, a, "%");
}

lval* builtin_head(lenv* e, lval* a) {
    LASSERT_ONEARG(a, "head", 1);
    LASSERT_TYPE(a, "head", 0, LVAL_QEXPR);
    LASSERT_ELIST(a, "head", 0);

    lval* v = lval_take(a, 0);
    while (v->count > 1) {lval_del(lval_pop(v, 1));}
    return v;
}

lval* builtin_tail(lenv* e, lval* a) {
    LASSERT_ONEARG(a, "tail", 1);
    LASSERT_TYPE(a, "tail", 0, LVAL_QEXPR);
    LASSERT_ELIST(a, "tail", 0);

    lval*  v= lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lenv* e, lval* a) {
    LASSERT_ONEARG(a, "eval", 1);
    LASSERT_TYPE(a, "eval", 0, LVAL_QEXPR);

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

lval* builtin_join(lenv* e, lval* a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT_TYPE(a, "join", 0, LVAL_QEXPR);
    }

    lval* x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

lval* builtin_def(lenv* e, lval* a) {
    LASSERT_TYPE(a, "def", 0, LVAL_QEXPR)

    lval* syms = a->cell[0];

    for (int i=0; i < syms->count; i ++) {
        LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
                "Function 'def' cannot define non-symbol. Got %s and expected %s.",
                ltype_name(syms->cell[i]->type), ltype_name(LVAL_SYM));
    }

    LASSERT(a, syms->count == a->count-1,
            "Function def cannot define incorrect number of values to symbols.");

    for (int i=0; i < syms->count; i++) {
        lenv_put(e, syms->cell[i], a->cell[i+1]);
    }

    lval_del(a);
    return lval_sexpr();
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* k = lval_sym(name);
    lval* v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k);
    lval_del(v);
}

void lenv_add_builtins(lenv* e) {
    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
    lenv_add_builtin(e, "%", builtin_mod);
}

lval* lval_eval_sexpr(lenv* e, lval* v) {
    for (int i=0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    for (int i=0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) {return lval_take(v, i);}
    }

    if (v->count == 0) {return v;}

    if (v->count == 1) {return lval_take(v, 0);}

    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        lval_del(v);
        lval_del(f);
        return lval_err("first element is not a function, got '%s'",
                        ltype_name(f->type));
    }

    lval* result = f->fun(e, v);
    lval_del(f);
    return result;
}

static char* lisp_name = "plisp";
static char* prompt_prefix = "> ";

int main(int argc, char** argv) {
    //Create parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* plisp = mpc_new("plisp");

    //Define them with language
    mpca_lang(MPCA_LANG_DEFAULT,
            "                                                     \
            number   : /-?[0-9]+[.]?[0-9]*/ ;                     \
            symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&%]+/ ;        \
            sexpr    : '(' <expr>* ')' ;                          \
            qexpr    : '{' <expr>* '}' ;                          \
            expr     : <number> | <symbol> | <sexpr> | <qexpr> ;  \
            plisp    : /^/ <expr>* /$/ ;                          \
            ",
            Number, Symbol, Sexpr, Qexpr, Expr, plisp);

    printf("%s Version 0.0.0.0.1\n", lisp_name);
    puts("Press Ctrl+c do Exit\n");

    lenv* e = lenv_new();
    lenv_add_builtins(e);

    while (1) {
        printf("%s%s", lisp_name, prompt_prefix);
        char* input = readline("");
        if (input == NULL) {break;}
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, plisp, &r)) {
            lval* y = lval_eval(e, lval_read(r.output));
            lval_println(y);
            lval_del(y);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        free(input);
    }

    lenv_del(e);
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, plisp);

    return 0;
}
