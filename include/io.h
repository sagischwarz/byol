#pragma once
#include "lval.h"
#include "../lib/mpc.h"

lval* lval_read_num(mpc_ast_t* t);

lval* lval_read(mpc_ast_t* t);

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close);

void lval_print(lval* v);

void lval_println(lval* v);
