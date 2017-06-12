#ifndef __LENV_H__
#define __LENV_H__
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "lval.h"

struct lenv {
    int count;
    char** syms;
    lval** vals;
};

lenv* lenv_new(void);
void lenv_del(lenv* e);
lval* lenv_get(lenv* e, lval* k);
void lenv_put(lenv* e, lval* k, lval* v);
#endif