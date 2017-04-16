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


enum {LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR , LVAL_QEXPR};

typedef struct lval {
    int type;
    double num;
    char* err;
    char* sym;
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

lval* lval_eval_sexpr(lval* v);

lval* lval_eval(lval* v) {
    if (v->type == LVAL_SEXPR) {return lval_eval_sexpr(v);}
    return v;
}

int is_int(double val) {
    return ceilf(val) == val;
}

lval* builtin_op(lval* a, char* op) {
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

lval* builtin_head(lval* a) {
    LASSERT_ONEARG(a, "head");
    LASSERT_FIRST_QEXPR(a, "head");
    LASSERT_ELIST(a, "head");

    lval* v = lval_take(a, 0);
    while (v->count > 1) {lval_del(lval_pop(v, 1));}
    return v;
}

lval* builtin_tail(lval* a) {
    LASSERT_ONEARG(a, "tail");
    LASSERT_FIRST_QEXPR(a, "tail");
    LASSERT_ELIST(a, "tail");

    lval*  v= lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

lval* builtin_list(lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lval* a) {
    LASSERT_ONEARG(a, "eval");
    LASSERT_FIRST_QEXPR(a, "eval");

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

lval* lval_join(lval* x, lval* y) {
    while (y->count > 0) {
        x = lval_add(x, lval_pop(y, 0));
    }

    lval_del(y);
    return x;
}

lval* builtin_join(lval* a) {
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

lval* builtin_len(lval* a) {
    LASSERT_ONEARG(a, "head");
    LASSERT_FIRST_QEXPR(a, "head");
    LASSERT_ELIST(a, "head");

    lval* x = lval_num(a->cell[0]->count);
    lval_del(a);
    return x;
}

lval* builtin_init(lval* a) {
    LASSERT_ONEARG(a, "init");
    LASSERT_FIRST_QEXPR(a, "init");
    LASSERT_ELIST(a, "init");

    lval*  v= lval_take(a, 0);
    lval_del(lval_pop(v, v->count -1));
    return v;
}

lval* builtin(lval* a, char* func) {
    if (strcmp("list", func) == 0) {return builtin_list(a);}
    if (strcmp("head", func) == 0) {return builtin_head(a);}
    if (strcmp("tail", func) == 0) {return builtin_tail(a);}
    if (strcmp("init", func) == 0) {return builtin_init(a);}
    if (strcmp("join", func) == 0) {return builtin_join(a);}
    if (strcmp("eval", func) == 0) {return builtin_eval(a);}
    if (strcmp("len", func) == 0) {return builtin_len(a);}
    if (strstr("+-/*", func)) {return builtin_op(a, func);}
    lval_del(a);
    return lval_err("Unknown function!");
}

lval* lval_eval_sexpr(lval* v) {
    for (int i=0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    for (int i=0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) {return lval_take(v, i);}
    }

    if (v->count == 0) {return v;}

    if (v->count == 1) {return lval_take(v, 0);}

    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f);
        lval_del(v);
        return lval_err("S-expression does not start with a symbol!");
    }

    lval* result = builtin(v, f->sym);
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
            symbol   : \"list\" | \"head\" | \"tail\"             \
                     | \"join\" | \"eval\" | \"len\"              \
                     | \"init\"                                   \
                     | '+' | '-' | '*' | '/' | '%' ;              \
            sexpr    : '(' <expr>* ')' ;                          \
            qexpr    : '{' <expr>* '}' ;                          \
            expr     : <number> | <symbol> | <sexpr> | <qexpr> ;  \
            plisp    : /^/ <expr>* /$/ ;                          \
            ",
            Number, Symbol, Sexpr, Qexpr, Expr, plisp);

    printf("%s Version 0.0.0.0.1\n", lisp_name);
    puts("Press Ctrl+c do Exit\n");

    while (1) {
        printf("%s%s", lisp_name, prompt_prefix);
        char* input = readline("");
        if (input == NULL) {break;}
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, plisp, &r)) {
            lval* y = lval_eval(lval_read(r.output));
            lval_println(y);
            lval_del(y);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        free(input);
    }
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, plisp);
    return 0;
}
