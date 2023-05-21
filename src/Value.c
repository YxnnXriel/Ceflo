#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/Value.h"
#include "include/debug.h"
#include "include/error.h"
#include "include/typer.h"


#define RETURN_NONE newAst(AST_NONE)

#define FOR_GLOBAL(value) ((value)->variable_forGlobal = true)
#define RESET_GLOBAL(value) ((value)->variable_forGlobal = false)

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
    FOR_GLOBAL(value);
    ast_t * item = visitor_visit(visitor,value);
    RESET_GLOBAL(value);
    List_Append(list,ast_copy(item,visitor->global_scope));
    return item;
}

ast_t * value_List_get(visitor_t * visitor , list_t *list , ast_t * value){
    
    FOR_GLOBAL(value);
    ast_t * l = visitor_visit(visitor,value);
    RESET_GLOBAL(value);

    if(l->type == AST_NUMBER){
        if(l->value.as.numberValue < list->Size)
            return List_Get(list,l->value.as.numberValue);
        
        return RETURN_NONE;
    }

    return RETURN_NONE;
}

ast_t * value_List_deleted(visitor_t * visitor , list_t *list , ast_t * value){


    FOR_GLOBAL(value);
    ast_t * l = visitor_visit(visitor,value);
    RESET_GLOBAL(value);

    if(l->type == AST_NUMBER){

        List_Del_without_supp(
            list,
            l->value.as.numberValue
        );

        return l;            
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


ast_t * value_List_filter(visitor_t * visitor , list_t *list , ast_t * value){

    

    ast_t * func = visitor_visit(visitor,List_Ast_Get(&value->function_call_args,0));

    list_ast_t new = List_Ast_Cpy_Without_Index(&value->function_call_args,0);

    ast_t * cpy_args = newAst(AST_FUNCTION_CALL);
    cpy_args->function_call_args = new;
    
    
    if(func->type == AST_LFUNC){
        return visitor_visit_function(
            visitor,
            func->value.as.lfuncValue,
            cpy_args
        );
    }


    return RETURN_NONE;

}







// ======================================== $STR ======================================================================


ast_t * value_str_switch_function(visitor_t * visitor,string_t * str, ast_t *value){

    if(value->type == AST_FUNCTION_CALL){
        for (int i = 0; i < VALUE_STRING_ATTR; i++){
            if(strcmp(attr_value_str[i].name,value->function_call_name)==0){
                return attr_value_str[i].func(visitor,str,value);
            }
        }
        
    }

    return NULL;

}

ast_t *Value_String_lenght(visitor_t *visitor , string_t * str , ast_t * value){
    ast_t * val = newAstWithLine(AST_NUMBER,value->line,value->colone);
    newValue(&val->value,NUMBER);
    val->value.as.numberValue = str->lenght;

    return val;
}

ast_t *Value_String_format(visitor_t *visitor , string_t * str , ast_t * value){


    char buffer[BUFSIZ];

    int n = 0;

    for (int i = 0; i < str->lenght; i++){
        if(String_index(str,i) == '{'){
            if(i+1 != str->lenght && String_index(str,i+1) == '}'){
                ast_t * item = visitor_visit(visitor,List_Ast_Get(&value->function_call_args,n));
                typer_caste_value(AST_STRING,&item);
                strcat(buffer,AS_STRING(item->value));
                i++;
                n++;
            }else{
                strcat(buffer,"{");
            }
        }else{
            sprintf(buffer + strlen(buffer), "%c", String_index(str,i));
        }
    }

    ast_t * string = newAst(AST_STRING);
    string->value.as.stringValue = new_String(buffer);
    return string;

}