#!/bin/sh
# ./compile.sh nom gc.c [options] : Ceflo_ref avec ce gc.c à la place de src/gc.c
NOM=$1; GC=$2; shift 2
SRC=../../../src
gcc -std=c11 -D_GNU_SOURCE -Wall -Wextra -I$SRC -I$SRC/include "$@" -o $NOM $GC \
    $(ls $SRC/*.c | grep -v '/gc.c$') -rdynamic -lm -ldl
