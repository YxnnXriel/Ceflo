#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/Value.h"

#include "include/error.h"



#define RETURN_NONE newAst(AST_NONE)

static ast_t * value_list_for(int index_function , visitor_t * visitor , list_t *list , ast_t * value ){
    ast_t *get = NULL ;

    if(index_function == 0){
        if(value->function_call_args.size > 1){
            get = newAst(AST_LIST);
            newValue(&get->value,LIST);
            get->value.as.listValue = __init__list__(sizeof(ast_t*));
        }
    }



    for (int i = 0; i < (int)value->function_call_args.size; i++){
        ast_t *val =  attr_value_list[index_function].func_list_name(visitor,list,List_Ast_Get(&value->function_call_args,i));
        if(index_function == 0){
            if(value->function_call_args.size == 1)
                return val;
            else
                List_Append(get->value.as.listValue,val);
        }
    }
    return get != NULL ? get: value;
}

static ast_t * value_list_switch_function_call(int index_function , visitor_t * visitor , list_t *list , ast_t * value){

    switch (index_function){
        case 0:
        case 1:
        case 2:
            return value_list_for(index_function,visitor,list,value);
        default:
            return attr_value_list[index_function].func_list_name(visitor,list,value);

    }

    return value;
    
}


static bool is_null_args(ast_t *args){
    return args->function_call_args.size == 0 ? true: false; 
}

ast_t * value_List_lenght(visitor_t * visitor , list_t *list , ast_t * value){

    if(!is_null_args(value)) return RETURN_NONE;

    ast_t * lenght = newAst(AST_NUMBER);
    newValue(&lenght->value,NUMBER);
    lenght->value.as.numberValue = list->Size;

    return lenght;

}

ast_t * value_List_append(visitor_t * visitor , list_t *list , ast_t * value){

    ast_t * item = visitor_visit(visitor,value);
    List_Append(list,value);
    return item;
}

ast_t * value_List_get(visitor_t * visitor , list_t *list , ast_t * value){

    ast_t *none = newAst(AST_NONE); 

    if(value->type == AST_NUMBER){
        if(value->value.as.numberValue < list->Size)
            return List_Get(list,value->value.as.numberValue);
        return none;
    }

    return none;
}

ast_t * value_List_deleted(visitor_t * visitor , list_t *list , ast_t * value){

    if(value->type == AST_NUMBER){

        List_Del_without_supp(
            list,
            value->value.as.numberValue
        );

        return value;            
    }

    return newAst(AST_NONE);
}
ast_t * value_List_print(visitor_t * visitor , list_t *list , ast_t * value){
    if(!is_null_args(value)) return RETURN_NONE;

        printlistByeList(list,visitor);
    return value;
}

ast_t * value_list_switch_function(visitor_t * visitor,list_t *list , ast_t *value){


    if(value->type == AST_FUNCTION_CALL){
    
        for (int i = 0; i < VALUE_LIST_ATTR; i++){
            if(strcmp(attr_value_list[i].name,value->function_call_name) == 0){
                return value_list_switch_function_call(i,visitor,list,value);
            }
        }

    }

    return NULL;
}