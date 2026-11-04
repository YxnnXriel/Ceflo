#include <stdlib.h>

#include "include/scope.h"

scope_t *newScope(scope_t *parent){
    scope_t *scope = gc_alloc(sizeof(*scope),GC_SCOPE);
    scope->scope_parent = parent;
    return scope;
}

void freeScope(scope_t *scope){
    Hash_Free(&scope->variables);
    List_Free(scope->exports);
}

hash_t *scope_get(scope_t *scope, const char *name){
    return Hash_Get(&scope->variables,name);
}

hash_t *scope_lookup_hashed(scope_t *scope, const char *name, uint32_t hash){
    for(; scope != NULL; scope = scope->scope_parent){
        hash_t *item = Hash_Get_Hashed(&scope->variables,name,hash);
        if(item != NULL)
            return item;
    }
    return NULL;
}

hash_t *scope_lookup(scope_t *scope, const char *name){
    return scope_lookup_hashed(scope,name,Hash_Name(name));
}

hash_t *scope_define(scope_t *scope, const char *name, struct VALUE_STRUCT *value){
    hash_t *item = Hash_Set(&scope->variables,name,value);
    item->type_name = NULL;
    item->is_const = false;
    return item;
}
