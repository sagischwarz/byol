cmake_minimum_required(VERSION 3.7)
project(byol)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES src/parsing.c lib/mpc.h lib/mpc.c include/lenv.h src/lenv.c include/lval.h src/lval.c src/io.c include/io.h src/builtins.c include/builtins.h)
add_executable(byol ${SOURCE_FILES})
target_link_libraries(byol m readline)
