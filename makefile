mainmake: src/parsing.c
		cc -std=c99 -g -Wall src/parsing.c lib/mpc.c -ledit -lm -o bin/parsing
