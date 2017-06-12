mainmake: src/parsing.c
		mkdir -p bin
		cc -std=c99 -g -Wall -Werror-implicit-function-declaration src/lval.c src/lenv.c src/parsing.c lib/mpc.c -ledit -lm -o bin/parsing
