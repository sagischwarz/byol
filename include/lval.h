#pragma once
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

enum {LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_FUN, LVAL_SEXPR , LVAL_QEXPR};

typedef lval*(*lbuiltin)(lenv*, lval*);
lval* lval_num(double x);
lval* lval_err(char* fmt, ...);
lval* lval_sym(char* s);
lval* lval_fun(lbuiltin func);
lval* lval_sexpr(void);
lval* lval_qexpr(void);
void lval_del(lval* v);
lval* lval_add(lval* v, lval* x);
lval* lval_copy(lval* v);
lval* lval_join(lval* x, lval* y);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);

typedef struct lval {
    int type;
    double num;
    char* err;
    char* sym;
    lbuiltin fun;
    int count;
    struct lval** cell;
} lval;
