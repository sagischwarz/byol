#include "../lib/mpc.h"

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
#include <editline/readline.h>
#endif

#define LASSERT(args, cond, err) \
    if (!(cond)) {lval_del(args); return lval_err(err);}

#define LASSERT_ONEARG(args, func) \
    LASSERT(args, args->count == 1, \
            "Function '" func "' passed too many arguments!")

#define LASSERT_ELIST(args, func) \
    LASSERT(args, a->cell[0]->count != 0, \
            "Function '" func "' passed {}!")

#define LASSERT_FIRST_QEXPR(args, func) \
    LASSERT(args, args->cell[0]->type == LVAL_QEXPR,\
        "Function '" func "' passed incorrect type!")

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

enum {LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_FUN, LVAL_SEXPR , LVAL_QEXPR};

typedef lval*(*lbuiltin)(lenv*, lval*);

typedef struct lval {
    int type;
    double num;
    char* err;
    char* sym;
    lbuiltin fun;
    int count;
    struct lval** cell;
} lval;

lval* lval_num(double x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

lval* lval_err(char* m) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

lval* lval_fun(lbuiltin func) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = func;
    return v;
}

lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lval* v) {
    switch (v->type) {
        case LVAL_NUM:
            break;
        case LVAL_FUN:
            break;
        case LVAL_ERR:
            free(v->err);
            break;
        case LVAL_SYM:
            free(v->sym);
            break;
        //If Qexpr or Sexpr then delete all elements inside
        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
            break;
    }

    free(v);
}

lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    double x = strtod(t->contents, NULL);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) {
        return lval_read_num(t);
    }
    if (strstr(t->tag, "symbol")) {
        return lval_sym(t->contents);
    }

    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) {
        x = lval_sexpr();
    }
    if (strstr(t->tag, "sexpr")) {
        x = lval_sexpr();
    }
    if(strstr(t->tag, "qexpr")) {
        x = lval_qexpr();
    }

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) {continue;}
        if (strcmp(t->children[i]->contents, ")") == 0) {continue;}
        if (strcmp(t->children[i]->contents, "{") == 0) {continue;}
        if (strcmp(t->children[i]->contents, "}") == 0) {continue;}
        if (strcmp(t->children[i]->tag, "regex") == 0) {continue;}
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);

        if (i != (v->count - 1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lval* v) {
    int prec = (ceilf(v->num) == v->num) ? 0 : 2;
    switch (v->type) {
        case LVAL_FUN:
            printf("<function>");
            break;
        case LVAL_NUM:
            printf("%.*f", prec, v->num);
            break;
        case LVAL_ERR:
            printf("Error %s", v->err);
            break;
        case LVAL_SYM:
            printf("%s", v->sym);
            break;
        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;
        case LVAL_QEXPR:
            lval_expr_print(v, '{', '}');
            break;
    }
}

void lval_println(lval* v) {
    lval_print(v);
    putchar('\n');
}

lval* lval_pop(lval* v, int i) {
    lval* x = v->cell[i];

    //Shift memory after the item at "i" over the top
    memmove(&v->cell[i], &v->cell[i+1],
            sizeof(lval*) * (v->count-i-1));

    v->count--;

    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return x;
}

lval* lval_take(lval* v, int i) {
    lval* x = lval_pop(v, i);
    lval_del(v);
    return x;
}

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

lval* lval_copy(lval* v) {
    lval* x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type) {
        case LVAL_FUN:
            x->fun = v->fun;
            break;
        case LVAL_NUM:
            x->num = v->num;
            break;
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err);
            break;
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval*) * x->count);
            for (int i=0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }
    return x;
}

int is_int(double val) {
    return ceilf(val) == val;
}

struct lenv {
    int count;
    char** syms;
    lval** vals;
};

lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv* e) {
    for(int i=0; i < e->count; i++) {
        lval_del(e->vals[i]);
        free(e->syms[i]);
    }

    free(e->syms);
    free(e->vals);
    free(e);
}

lval* lenv_get(lenv* e, lval* k) {
    for (int i=0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }

    return lval_err("unbound symbol!");
}

void lenv_put(lenv* e, lval* k, lval* v) {
    for (int i=0; i < e->count; i++) {
        if (strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);

    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->sym)+1);
    strcpy(e->syms[e->count-1], k->sym);
}



lval* builtin_op(lenv* e, lval* a, char* op) {
    for (int i=0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operate on non-number!");
        }
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
                return lval_err("Modulo works only on integers.");
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
    LASSERT_ONEARG(a, "head");
    LASSERT_FIRST_QEXPR(a, "head");
    LASSERT_ELIST(a, "head");

    lval* v = lval_take(a, 0);
    while (v->count > 1) {lval_del(lval_pop(v, 1));}
    return v;
}

lval* builtin_tail(lenv* e, lval* a) {
    LASSERT_ONEARG(a, "tail");
    LASSERT_FIRST_QEXPR(a, "tail");
    LASSERT_ELIST(a, "tail");

    lval*  v= lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lenv* e, lval* a) {
    LASSERT_ONEARG(a, "eval");
    LASSERT_FIRST_QEXPR(a, "eval");

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

lval* lval_join(lval* x, lval* y) {
    while (y->count > 0) {
        x = lval_add(x, lval_pop(y, 0));
    }

    lval_del(y);
    return x;
}

lval* builtin_join(lenv* e, lval* a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT_FIRST_QEXPR(a, "join");
    }

    lval* x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

lval* builtin_def(lenv* e, lval* a) {
    LASSERT_FIRST_QEXPR(a, "def")

    lval* syms = a->cell[0];

    for (int i=0; i < syms->count; i ++) {
        LASSERT(a, syms->cell[i]->type == LVAL_SYM,
                "Function 'def' cannot define non-symbol.");
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
        return lval_err("first element is not a function");
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
