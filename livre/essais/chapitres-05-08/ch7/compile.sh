#!/bin/sh
# ./compile.sh etape1 : compile l'interpréteur avec ce visitor à la place de src/visitor.c
SRC=../../../src
gcc -std=c11 -D_GNU_SOURCE -Wall -Wextra -O2 -I$SRC/include -o $1 $1.c \
    $(ls $SRC/*.c | grep -v '/visitor.c$') -rdynamic -lm -ldl
