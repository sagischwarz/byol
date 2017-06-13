#include <math.h>
#include <string.h>
#include "../include/builtins.h"
#include "../include/lenv.h"

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

int is_int(double val) {
    return ceilf(val) == val;
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
