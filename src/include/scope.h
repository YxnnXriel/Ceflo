#ifndef __SCOPE_H__
#define __SCOPE_H__

#include "ast.h"
#include "list.h"
#include "str.h"

typedef struct SCOPE_STRUCT{
    stk_t  variable_definition;
    stk_t function_definition;
    stk_t struct_definition;

    listString_t * variable_names;
    listString_t * function_names;
    listString_t * struct_names;

    int countScope;
    struct SCOPE_STRUCT *scope_parent;
}scope_t;


scope_t* newScope();
void freeScope(scope_t * scope);
void freeLscope(void *x);

ast_t *scope_get_variable_definition(scope_t * scope , const char *vardef);
void scope_append_variable_definition(scope_t *scope , const char *key , ast_t *value);

ast_t *scope_get_function_definition(scope_t * scope , const char *funcdef);
void scope_append_function_definition(scope_t *scope , const char *key , ast_t *value);


ast_t *scope_get_struct_definition(scope_t * scope , const char *structdef);
void scope_append_struct_definition(scope_t *scope , const char *key , ast_t *value);
int scope_cpy_with_scope(scope_t *end , scope_t *ref);


#endif