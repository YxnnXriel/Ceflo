#include <stdio.h>
#include "token.h"
int main(void){ printf("%zu %d\n", sizeof(token_t), TOKEN_EOF + 1); return 0; }
