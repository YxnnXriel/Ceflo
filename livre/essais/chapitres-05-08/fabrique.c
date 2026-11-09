#include <stdio.h>
#include "Value.h"

int main(void){
    value_t *a = makeNumber(1);
    value_t *b = makeNumber(1);
    printf("makeNumber(1) deux fois : %p et %p\n", (void*)a, (void*)b);
    printf("makeBool(true) deux fois : %p et %p\n", (void*)makeBool(true), (void*)makeBool(true));
    printf("makeNone() deux fois : %p et %p\n", (void*)makeNone(), (void*)makeNone());
    printf("égales ? %s\n", Value_equals(a, b) ? "oui" : "non");
    return 0;
}
