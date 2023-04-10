/**
 * @file token.c
 * @author Yann Ariel (yannariel13@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/token.h"
#include "include/common.h"



/**
 * @brief OPP new token
 * 
 * @param type 
 * @param value 
 * @return token_t* 
 */
token_t * newToken(TokenEnum type , const char *value){
    token_t * token = calloc(1,sizeof(token_t*));
    token->type = type;
    token->value = calloc(strlen(value),sizeof(char*));
    strcpy(token->value , value);

    return token;
}







/**
 * @brief Copy Token
 * 
 * @param tok 
 * @return token_t 
 */
token_t token_cpy(token_t tok){
    token_t token;
    token.type = tok.type;
    allowChar(&token.value,tok.value);

    return token;
}







/**
 * @brief Del token
 * 
 * @param token 
 * @return int 
 */
int freeToken(token_t *token){
    
    if(token->value){
        if(token->type <= 24){
            free(token->value);
        }

    }

    return 1;
}