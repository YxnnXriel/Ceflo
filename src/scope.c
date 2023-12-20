#include <stdio.h>
#include <stdlib.h>
#include "include/scope.h"
#include "include/ast.h"
#include "include/error.h"


static scope_t *current_scope;
static scope_t *prev_scope;

static scope_t *initScope(){
    return current_scope;
}

scope_t * GetFirstScope(){
    return initScope();
}

void ResetFirstScope(){
    current_scope = NULL;
    prev_scope = NULL;
}





scope_t* newScope(){
    scope_t *scope = calloc(1,sizeof(*scope));
    __init__stk_hash(&scope->function_definition);
    __init__stk_hash(&scope->variable_definition);
    __init__stk_hash(&scope->variable_definition_global);
    scope->scope_parent = NULL;
    scope->countScope = -1;



    if(prev_scope == NULL){

        scope->next = NULL;
        current_scope = scope;
        prev_scope = scope;

    }else{

        current_scope->prev = scope;
        scope->next = current_scope;
        current_scope = scope;
    }

    return scope;

}

ast_t * scope_get_variable_definition_global(scope_t * scope , const char *vardef){
    ast_t * value = NULL;
    if(scope->variable_definition_global.Size == 0){
            return NULL;
    }else{
        value = (ast_t*)Hash_Get(&scope->variable_definition_global,vardef);
        if(value != NULL)
            value->scope = scope;
        
        return value;
    }
}


ast_t *scope_get_variable_definition(scope_t * scope , const char *vardef){
    ast_t * value = NULL;

    if(scope->variable_definition.Size != 0){
        value = (ast_t*)Hash_Get(&scope->variable_definition,vardef);
        if(value != NULL){
            if(value->variable_definition_ptr == false){
                value->scope = scope;
            }
            return value;
        }
    }
    
    if(scope->variable_definition_global.Size != 0){
        return scope_get_variable_definition_global(scope,vardef);
    }


    return value;
}

ast_t ** scope_get_variable_definition_ptr(scope_t * scope , const char *vardef){
    hash_t * hash = NULL;
    if(scope->variable_definition.Size != 0){
        hash = Hash_Get_With_addr(&scope->variable_definition,vardef);
        if(hash != NULL){
            ast_t ** value = (ast_t**)&(hash->value);
            (*value)->scope = scope;
            // (*value)->assignement_variable_name = vardef;
            return value;
        }
    }

    if(scope->variable_definition_global.Size != 0){    
        hash = Hash_Get_With_addr(&scope->variable_definition_global,vardef);
        if(hash != NULL){
            ast_t ** value = (ast_t**)&(hash->value);
            (*value)->scope = scope;
            // (*value)->assignement_variable_name = vardef;
            return value;
        }
    }

    return NULL;
}

void scope_modif_variable_definition(scope_t *scope , const char *key , ast_t *value ){
    ast_t * hold = scope_get_variable_definition(
        scope,
        key
    );

    if(hold && !hold->variable_definition_is_global ){
        Hash_Modif(&scope->variable_definition,key,value);
    }else if( hold && hold->variable_definition_is_global ){
        Hash_Modif(&scope->variable_definition_global,key,value);
    }
}

void scope_append_variable_definition(scope_t *scope , const char *key , ast_t *value){
    ast_t * stk_val = scope_get_variable_definition(scope,key);

    if(value->variable_definition_is_global && stk_val == NULL){
        Hash_Append(&scope->variable_definition_global,key,value);
        // printf("globale size[%d]",scope->variable_definition_global.Size);
        return;
    }

    if(stk_val != NULL){
        // update
        Hash_Modif(&scope->variable_definition,key,value);
        // stk_val->value = value->value;
        // stk_val->is_for_variable = false;
        // stk_val->variable_const = value->variable_const;
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

void Print_Scope(scope_t * scope , const char *chaine){
    if(chaine != NULL){
        printf(" ===== %s ===== \n",chaine);
    }

    ast_t * node = NULL;

    printf(" **** VARIABLE **** \n");
    for (int i = 0; i < scope->variable_definition.Size; i++){
        hash_t * item = Hash_Get_by_index(&scope->variable_definition,i);
        node = (ast_t*)item->value;
        printf("    %s\n",node->variable_definition_name); 
    }

    printf(" **** GLOBAL VARIABLE **** \n");
    for (int i = 0; i < scope->variable_definition_global.Size; i++){
        hash_t * item = Hash_Get_by_index(&scope->variable_definition_global,i);
        node = (ast_t*)item->value;
        printf("    %s\n",node->variable_definition_name); 
    }


    printf(" **** FUNCTION **** \n");
    for (int i = 0; i < scope->function_definition.Size; i++){
        hash_t * item = Hash_Get_by_index(&scope->function_definition,i);
        node = (ast_t*)item->value;
        printf("    %s\n",node->function_definition_name); 
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
        if(scope->variable_definition_global.Size != 0){
            Hash_Del(&scope->variable_definition_global);
        }
    }
}


void clearScope(){
    scope_t * scope = GetFirstScope();
    while (scope != NULL){
        scope_t *sup = scope;
        scope = scope->next;
        freeScope(sup);
        free(sup);
    }
    
}

void freeSingleScope(scope_t *scope){

    if(scope != NULL){
        if(scope->next == NULL){
            prev_scope = scope->prev;
            prev_scope->next = NULL;
        }else if(scope->prev == NULL){
            current_scope = scope->next;
            current_scope->prev = NULL;
        }else{
            scope->next->prev = scope->prev;
            scope->prev->next = scope->next;
        }
        freeScope(scope);
        free(scope);
    }
}
