// Combien d'octets pour une valeur, avant et après ?
#include <stdio.h>
#include "Value.h"

int main(void){
    printf("value_t (le visitor) : %zu octets\n",sizeof(value_t));
    return 0;
}
