/**
 * @file token.c
 * @author Yann Ariel (yannariel13@gmail.com)
 * @copyright Copyright (c) 2023
 */

#include <stdlib.h>

#include "include/token.h"

#define GENERATE_TOKEN_STRING(TOKEN, TEXT) TEXT,

static const char *TOKEN_TO_STRING[] = {
    FOREACH_TOKEN(GENERATE_TOKEN_STRING)
};

const char *token_to_string(TokenEnum type){
    return TOKEN_TO_STRING[type];
}

bool token_owns_value(TokenEnum type){
    return type <= TOKEN_STRING;
}

void freeToken(token_t *token){
    if(token_owns_value(token->type))
        free(token->value);
    token->value = NULL;
}
