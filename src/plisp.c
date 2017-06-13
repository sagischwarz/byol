#include "../lib/mpc.h"
#include "../include/lval.h"
#include "../include/lenv.h"
#include "../include/io.h"
#include "../include/builtins.h"

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
