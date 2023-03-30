#ifndef __SCOPE_H__
#define __SCOPE_H__

#include "ast.h"
#include "list.h"
#include "str.h"

typedef struct SCOPE_STRUCT{
    stk_t  variable_definition;
    stk_t function_definition;
    int countScope;
}scope_t;


scope_t* newScope();
void freeScope(scope_t * scope);

ast_t *scope_get_variable_definition(scope_t * scope , const char *vardef);
void scope_append_variable_definition(scope_t *scope , const char *key , ast_t *value);

ast_t *scope_get_function_definition(scope_t * scope , const char *funcdef);
void scope_append_function_definition(scope_t *scope , const char *key , ast_t *value);

#endif