#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/token.h"
#include "include/common.h"

token_t * newToken(TokenEnum type , const char *value){
    token_t * token = calloc(1,sizeof(token_t*));
    token->type = type;
    token->value = calloc(strlen(value),sizeof(char*));
    strcpy(token->value , value);

    return token;
}


token_t token_cpy(token_t tok){
    token_t token;
    token.type = tok.type;
    allowChar(&token.value,tok.value);

    return token;
}

int freeToken(token_t *token){
    
    if(token->value){
        if(token->type <= 24){
            free(token->value);
        }

    }

    return 1;
}