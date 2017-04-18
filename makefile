mainmake: src/parsing.c
		cc -std=c99 -g -Wall -Werror-implicit-function-declaration src/parsing.c lib/mpc.c -ledit -lm -o bin/parsing
