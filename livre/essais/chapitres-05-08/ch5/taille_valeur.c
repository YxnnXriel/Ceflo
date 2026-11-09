#include <stdio.h>
#include "Value.h"

int main(void){
    printf("gc_object_t : %zu octets\n", sizeof(gc_object_t));
    printf("value_t     : %zu octets\n", sizeof(value_t));
    printf("double      : %zu octets\n", sizeof(double));
    return 0;
}
