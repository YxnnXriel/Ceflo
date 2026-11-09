#!/bin/sh
# ./compile.sh essai : les valeurs, les portées et les types, sans visitor ni GC
SRC=../../../src
gcc -std=c11 -D_GNU_SOURCE -Wall -Wextra -g -I$SRC/include -o $1 $1.c ../ch5/provisoire.c provisoire6.c \
    $SRC/Value.c $SRC/value_expr.c $SRC/map.c $SRC/scope.c $SRC/typer.c $SRC/ast.c \
    $SRC/str.c $SRC/list.c $SRC/common.c $SRC/error.c $SRC/token.c -lm
