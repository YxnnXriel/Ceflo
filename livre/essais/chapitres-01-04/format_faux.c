// Un appel à error_syntax qui se trompe d'argument : gcc le voit grâce à `format`.
#include "error.h"

int main(void){
    int line = 3;
    error_syntax("essai.cf",line,1,"ligne %s",line);
}
