#include <stdio.h>
#include "Value.h"
#include "value_expr.h"

static void montre(value_t *left, TokenEnum op, value_t *right){
    value_t *result = value_expr(NULL, NULL, left, op, right);
    Value_print(NULL, stdout, result);
    printf("\n");
}

int main(void){
    ceflo_stack_init();
    montre(makeNumber(1), TOKEN_PLUS, makeString("a"));
    montre(makeChar('A'), TOKEN_PLUS, makeNumber(1));
    montre(makeString("ab"), TOKEN_MULT, makeNumber(3));
    montre(makeString("abc"), TOKEN_INF, makeString("abd"));
    montre(makeList(), TOKEN_MINUS, makeNumber(1));
    printf("jamais affiché\n");
    return 0;
}
