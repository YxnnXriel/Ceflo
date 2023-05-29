#include <stdio.h>
#include <stdlib.h>
#include "include/scope.h"
#include "include/ast.h"
#include "include/error.h"

scope_t* newScope(){
    scope_t *scope = calloc(1,sizeof(*scope));
    __init__stk_hash(&scope->function_definition);
    __init__stk_hash(&scope->variable_definition);
    scope->scope_parent = NULL;
    scope->countScope = -1;
    return scope;

}

ast_t *scope_get_variable_definition(scope_t * scope , const char *vardef){
    if(scope->variable_definition.Size == 0)
        return NULL;

    ast_t * value = (ast_t*)Hash_Get(&scope->variable_definition,vardef);
    if(value != NULL){
        value->scope = scope;
        value->assignement_variable_name = vardef;
    }

    return value;
}

void scope_append_variable_definition(scope_t *scope , const char *key , ast_t *value){
    ast_t * stk_val = scope_get_variable_definition(scope,key);



    if(stk_val != NULL){
        // update
        Hash_Modif(&scope->variable_definition,key,value);
        stk_val->is_for_variable = false;
    }else{
        Hash_Append(&scope->variable_definition,key,value);
    }
}

ast_t *scope_get_function_definition(scope_t * scope , const char *funcdef){
    // if(scope->function_definition.Size == 0)
    //     return NULL;


    ast_t * value = Hash_Get(&scope->function_definition,funcdef);
    if(value == NULL){
        value = Hash_Get(&scope->variable_definition,funcdef);
        return value != NULL && value->type == AST_LFUNC? value->value.as.lfuncValue:NULL;
    }


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
        Hash_Append(&scope->function_definition,value->function_definition_name,value);   
    }

}

ast_t *scope_get_struct_definition(scope_t * scope , const char *structdef){
    
    if(scope->struct_definition.Size == 0)
        return NULL;

    ast_t * value = Hash_Get(&scope->struct_definition,structdef);

    return value;

}
void scope_append_struct_definition(scope_t *scope , const char *key , ast_t *value){
    ast_t * stk_val = scope_get_struct_definition(scope,key);

    
    if(stk_val != NULL){
        printf("MultipleStructDefinition : <%s>\n",stk_val->struct_definition_name);
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
        Hash_Append(&scope->struct_definition,value->struct_definition_name,value);   
    }
}

static void Scope_intern_cpy(stk_t *Table , scope_t *end, void (*func)(scope_t *scope , const char *key , ast_t *value)){
    for (int i = 0; i < Table->Size; i++){
        hash_t* item = Hash_Get_by_index(Table,i);
        func(
            end,
            item->key,
            ast_copy((ast_t*)item->value,end)
        );
    }
} 

int scope_cpy_with_scope(scope_t *end , scope_t *ref){

    if(ref != NULL){

        Scope_intern_cpy(&ref->function_definition,end,scope_append_function_definition);
        Scope_intern_cpy(&ref->variable_definition,end,scope_append_variable_definition);
        Scope_intern_cpy(&ref->struct_definition,end,scope_append_struct_definition);


        // for (int i = 0; i < ref->function_definition.Size ; i++){
            
        //     ast_t * func = scope_get_function_definition(
        //         ref,
        //         ListString_Get(ref->function_names,i)
        //     );
        //     scope_append_function_definition(
        //         end,
        //         func->function_definition_name,
        //         ast_copy(func,end)
        //     );
        // }

        // // for (int i = 0; i < ref->variable_definition.Size ; i++){
            
        // //     ast_t * var = scope_get_variable_definition(
        // //         ref,
        // //         ListString_Get(ref->variable_names,i)
        // //     );
        // //     scope_append_variable_definition(
        // //         end,
        // //         ListString_Get(ref->variable_names,i),
        // //         ast_copy(var,end)
        // //     );
        // // }

        // for (int i = 0; i < ref->variable_definition.Size; i++){
        //     hash_t* item = Hash_Get_by_index(&ref->variable_definition,i);

        //     scope_append_variable_definition(
        //         end,
        //         item->key,
        //         ast_copy((ast_t*)item->value,end)
        //     );            
        // }

        // for (int i = 0; i < ref->struct_definition.Size ; i++){
            
        //     ast_t * structt = scope_get_struct_definition(
        //         ref,
        //         ListString_Get(ref->struct_names,i)
        //     );
        //     scope_append_struct_definition(
        //         end,
        //         ListString_Get(ref->struct_names,i),
        //         ast_copy(structt,end)
        //     );
        // }

        end->countScope = ref->countScope;
        end->scope_parent = ref->scope_parent;

    }

    return 1;

}

void freeLscope(void *x){

    scope_t *scope = (scope_t*)x;
    freeScope(scope);

}

void freeScope(scope_t * scope){
    if(scope != NULL){
        if(scope->function_definition.Size != 0)
            Hash_Del(&scope->function_definition);
        if(scope->variable_definition.Size != 0){
            Hash_Del(&scope->variable_definition);
        }
        if(scope->struct_definition.Size != 0){
            Hash_Del(&scope->struct_definition);
        }
        free(scope);
    }
}