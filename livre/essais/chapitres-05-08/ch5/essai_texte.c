#include <stdio.h>
#include "Value.h"

int main(void){
    ceflo_stack_init();
    value_t *l = makeList();
    Value_list_append(l, makeNumber(1));
    Value_list_append(l, makeString("1"));
    Value_list_append(l, makeChar('a'));
    Value_list_append(l, makeNumber(0.1 + 0.2));
    Value_list_append(l, l);

    Value_print(NULL, stdout, l);
    printf("\n");
    Value_print(NULL, stdout, makeNumber(1e15));
    printf(" ");
    Value_print(NULL, stdout, makeNumber(123456789012345));
    printf("\n");
    return 0;
}
