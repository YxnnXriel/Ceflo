#include <stdio.h>
#include <stdlib.h>
#include "include/scope.h"
#include "include/ast.h"
#include "include/error.h"
#include "include/clear.h"


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
    __init__stk_hash(&scope->struct_definition);
    scope->scope_parent = NULL;
    scope->is_for_obj = false;
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


ast_t *scope_get_variable_definition(scope_t * scope , const char *vardef){
    ast_t * value = NULL;

    value = (ast_t*)Hash_Get(&scope->variable_definition,vardef);
    
    return value;
}


void scope_append_variable_definition(scope_t *scope , const char *key , ast_t *value){
    ast_t * val = scope_get_variable_definition(scope,key);
    if(val != NULL){
        Hash_Modif(&scope->variable_definition,key,value);
    }else{
        Hash_Append(&scope->variable_definition,key,value);
    }
}

void scope_modif_variable_definition(scope_t * scope , const char *key , ast_t * value){
    Hash_Modif(&scope->variable_definition,key,value);
}



ast_t *scope_get_function_definition(scope_t * scope , const char *funcdef){
    // if(scope->function_definition.Size == 0)
    //     return NULL;

    ast_t * value = NULL; 
    value = Hash_Get(&scope->function_definition,funcdef);
    
    if(value == NULL){
        value = scope_get_variable_definition(scope,funcdef);
        if(value != NULL && value->type == AST_LFUNC){
            return value;
        }
        else
            return NULL;
    }


    return value;
}

void scope_append_function_definition(scope_t *scope , const char *key , ast_t *value){
    ast_t * stk_val = scope_get_function_definition(scope,key);

    
    if(stk_val != NULL){
        printf("MultipleFunctionDefinition : <%s>\n",stk_val->function_definition_name);
        Print_visitor_error(
            stk_val->line,
            stk_val->colone,
            "./Ceflo",
            "Deja Definit ici \n",0
        );

        Print_visitor_error(
            value->line,
            value->colone,
            "./Ceflo",
            "Redefinit encore la\n",0
        );
        clear_ceflo_clean();
        exit(EXIT_FAILURE);
    }else{
        Hash_Append(&scope->function_definition,key,value);   
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
            stk_val->line,
            stk_val->colone,
            "./Ceflo",
            "Deja Definit ici \n",0
        );

        Print_visitor_error(
            value->line,
            value->colone,
            "./Ceflo",
            "Redefinit encore la\n",0
        );
        clear_ceflo_clean();
        exit(EXIT_FAILURE);
    }else{
        Hash_Append(&scope->struct_definition,value->struct_definition_name,value);   
    }
}

static void Scope_intern_cpy(stk_t *Table , scope_t *end, void (*func)(scope_t *scope , const char *key , ast_t *value)){
    for (int i = 0; i < Table->Size; i++){
        hash_t* item = Hash_Get_by_index(Table,i,1);
        func(
            end,
            item->key,
            ast_copy((ast_t*)item->value,NULL)
        );
    }
} 

static void Scope_intern_get(stk_t *Table , scope_t *end, void (*func)(scope_t *scope , const char *key , ast_t *value)){
    for (int i = 0; i < Table->Size; i++){
        hash_t* item = Hash_Get_by_index(Table,i,1);
        func(
            end,
            item->key,
            item->value
        );
    }
} 

void Print_Scope(scope_t * scope , const char *chaine){
    if(chaine != NULL){
        printf(" ===== %s ===== \n",chaine);
    }

    ast_t * node = NULL;

    string_t * champ = new_String("****VARIABLE****");
    listString_t *list = new_ListString();

    // printf(" **** VARIABLE **** \n");
    if(scope->variable_definition.Size > 0){
        for (int i = 0; i < scope->variable_definition.Size; i++){
            hash_t *item = Hash_Get_by_index(&scope->variable_definition,i,1);
            node = (ast_t*)item->value;
            if(node != NULL){
                // printf("    %p - [%s] -> ",(void*)item,item->key); 
                // printValueAst(node);
                // printf("\n");
                ListString_Append(list,item->key);
            }
        }
        Table(champ,list);
        ListString_Clear(list);
        list = new_ListString();
    }



    if(scope->function_definition.Size > 0){
        String_Set(champ,"****FUNCTION****");
        // printf(" **** FUNCTION **** \n");
        for (int i = 0; i < scope->function_definition.Size; i++){
            hash_t *item = Hash_Get_by_index(&scope->function_definition,i,1);
            node = (ast_t*)item->value;
            if(node != NULL){
                ListString_Append(list,item->key);
                // printf("    %s\n",node->function_definition_name); 
            }
        }

        Table(champ,list);
        ListString_Clear(list);
        
        list = new_ListString();
    }


    if(scope->struct_definition.Size > 0){
        String_Set(champ,"****STRUCT****");
        // printf(" **** STRUCT **** \n");
        for (int i = 0; i < scope->struct_definition.Size; i++){
            hash_t *item = Hash_Get_by_index(&scope->struct_definition,i,1);
            node = (ast_t*) item->value;
            if(node != NULL)
                // printf("    %s\n",node->struct_definition_name);
                ListString_Append(list,item->key);
        }
        Table(champ,list);
    }

    ListString_Clear(list);
    String_free(champ);
}


int scope_cpy_with_scope(scope_t *end , scope_t *ref){

    if(ref != NULL){
        Scope_intern_cpy(&ref->function_definition,end,scope_append_function_definition);
        Scope_intern_cpy(&ref->variable_definition,end,scope_append_variable_definition);
        Scope_intern_cpy(&ref->struct_definition,end,scope_append_struct_definition);
        end->countScope = ref->countScope;
        end->scope_parent = ref->scope_parent;
    }

    return 1;
}

int scope_cpy_with_scope_without_count(scope_t * source , scope_t * destination){
    if(source != NULL){
        Scope_intern_get(&source->function_definition,destination,scope_append_function_definition);
        Scope_intern_get(&source->variable_definition,destination,scope_append_variable_definition);
        Scope_intern_get(&source->struct_definition,destination,scope_append_struct_definition);
        
        return 0;
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

ast_t *scope_found_element(scope_t * scope, const char *element , int *type){

    ast_t * value = scope_get_variable_definition(scope,element);

    if(value != NULL){
        *type = AST_VARIABLE;
        return value;
    }

    value = scope_get_function_definition(scope,element);

    if(value != NULL){
        *type = AST_FUNCTION_DEFINITION;
        return value;
    }
    
    value = scope_get_struct_definition(scope,element);
    
    if(value != NULL){
        *type = AST_STRUCT_DEFINITION;
        return value;
    }

    return NULL;
}


ast_t * scope_append_element(scope_t * scope, int type ,const char *key , ast_t * value ){
    switch (type){
        case AST_VARIABLE:
        case AST_VARIABLE_DEFINITION:
            scope_append_variable_definition(scope,key,value);
            break;
        case AST_FUNCTION_DEFINITION:
            scope_append_function_definition(scope,key,value);
            break;
        case AST_STRUCT_DEFINITION:
            scope_append_struct_definition(scope,key,value);
            break;
    }

    return value;
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
