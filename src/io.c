#include "../include/io.h"

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    double x = strtod(t->contents, NULL);
    return errno != ERANGE ? lval_num(x) :
           lval_err("Invalid number '%s'", t->contents);
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