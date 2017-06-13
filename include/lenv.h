#pragma once

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
void lenv_put(lenv* e, lval* k, lval* v);
void lenv_add_builtin(lenv* e, char* name, lbuiltin func);
void lenv_add_builtins(lenv* e);
lval* lenv_get(lenv* e, lval* k);
