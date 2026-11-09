#!/bin/sh
# ./compile.sh essai : un programme C avec les valeurs de CeFlo, sans visitor ni GC
SRC=../../../src
gcc -std=c11 -D_GNU_SOURCE -Wall -Wextra -g -I$SRC/include -o $1 $1.c provisoire.c \
    $SRC/Value.c $SRC/value_expr.c $SRC/map.c $SRC/scope.c \
    $SRC/str.c $SRC/list.c $SRC/common.c $SRC/error.c $SRC/token.c -lm
