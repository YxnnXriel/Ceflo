#include <stdio.h>
#include <stdlib.h>
#include "include/scope.h"
#include "include/ast.h"
#include "include/error.h"

scope_t* newScope(){
    scope_t *scope = calloc(1,sizeof(*scope));
    __init__stk_hash(&scope->function_definition);
    __init__stk_hash(&scope->variable_definition);
    scope->function_names = new_ListString();
    scope->scope_parent = NULL;
    scope->countScope = -1;
    return scope;

}

ast_t *scope_get_variable_definition(scope_t * scope , const char *vardef){
    if(scope->variable_definition.Size == 0)
        return NULL;

    ast_t * value = Hash_Get(&scope->variable_definition,vardef);

    return value;
}

void scope_append_variable_definition(scope_t *scope , const char *key , ast_t *value){
    ast_t * stk_val = scope_get_variable_definition(scope,key);


    if(stk_val != NULL){
        // update
        Hash_Modif(&scope->variable_definition,key,value);
    }else{
        Hash_Append(&scope->variable_definition,key,value);
    }
}

ast_t *scope_get_function_definition(scope_t * scope , const char *funcdef){
    if(scope->function_definition.Size == 0)
        return NULL;

    ast_t * value = Hash_Get(&scope->function_definition,funcdef);

    return value;
}

void scope_append_function_definition(scope_t *scope , const char *key , ast_t *value){
    ast_t * stk_val = scope_get_function_definition(scope,key);

    
    if(stk_val != NULL){
        printf("MultipleFunctionDefinition : <%s>\n",stk_val->function_definition_name);
        Print_visitor_error(
            0,
            0,
            "./Ceflo",
            "Deja Definit ici \n",0
        );

        Print_visitor_error(
            0,
            0,
            "./Ceflo",
            "Redefinit encore la\n",0
        );
        exit(1);
    }else{
        ListString_Append(scope->function_names,value->function_definition_name);
        Hash_Append(&scope->function_definition,value->function_definition_name,value);   
    }

}

int scope_cpy_with_scope(scope_t *end , scope_t *ref){


    for (int i = 0; i < ref->function_definition.Size ; i++){
        
        ast_t * func = scope_get_function_definition(
            ref,
            ListString_Get(ref->function_names,i)
        );
    

        // ast_t * func = (ast_t*) Hash_Get_by_index(
        //     &ref->function_definition,
        //     i
        // );

            
        // printf("%s \n",ListString_Get(ref->function_names,i));
        scope_append_function_definition(
            end,
            func->function_definition_name,
            func
        );
    }


    return 1;

}

void freeScope(scope_t * scope){
    if(scope != NULL){
        if(scope->function_definition.Size != 0)
            Hash_Del(&scope->function_definition);
        if(scope->variable_definition.Size != 0){
            Hash_Del(&scope->variable_definition);
        }
        if(scope->function_names){
            ListString_Clear(scope->function_names);
        }       
        free(scope);
    }
}