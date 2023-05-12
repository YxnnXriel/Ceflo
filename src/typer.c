#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "include/typer.h"
#include "include/ast.h"





ast_t * typer_caste_value( uint8_t to , ast_t *value){


    char str[50];
    ast_t *new = NULL;


    if(to == AST_BOOL){
        #define NEW_BOOL new = newAst(AST_BOOL)
        #define NBOOL new->value.as.booleanValue 
        switch (value->type){
            case AST_BOOL:return value;
            case AST_CHAR:
                NEW_BOOL;NBOOL = AS_CHAR(value->value);
                return new;
            case AST_NUMBER: 
                NEW_BOOL;NBOOL = AS_NUMBER(value->value);
                return new;
            case AST_STRING :
                NEW_BOOL;NBOOL = strlen(AS_STRING(value->value));
                return new;
            
        }
        #undef NEW_BOOL
        #undef NBOOL
    }

    if(to == AST_CHAR){
        #define NEW_CHAR new = newAst(AST_CHAR)
        #define NCHAR new->value.as.numberValue 
        switch (value->type){
            case AST_BOOL:
                NEW_CHAR;NCHAR = AS_BOOL(value->value);
                return new;
            case AST_CHAR: return value;
            case AST_NUMBER:
                NEW_CHAR;NCHAR = AS_NUMBER(value->value);
                return new;
            case AST_STRING : 
                NEW_CHAR;NCHAR = strlen(AS_STRING(value->value));
                return new;
            
        }
        #undef NEW_CHAR
        #undef NCHAR

    }
    
    if(to == AST_NUMBER){
        #define NEW_NUMBER new = newAst(AST_NUMBER)
        #define NNUMBER new->value.as.numberValue 
        switch (value->type){
            case AST_BOOL:
                NEW_NUMBER;NNUMBER = AS_BOOL(value->value);
                return new;
            case AST_CHAR:
                NEW_NUMBER;NNUMBER = AS_CHAR(value->value);
                return new;
            case AST_NUMBER: return value;
            case AST_STRING : 
                NEW_NUMBER;NNUMBER = strlen(AS_STRING(value->value));
                return new;
            
        }
        #undef NEW_NUMBER
        #undef NNUMBER

    }

    if(to == AST_STRING){
        #define NEW_STRING new = newAst(AST_STRING)
        #define NSTRING new->value.as.stringValue 
        switch (value->type){
            case AST_BOOL:
                NEW_STRING;NSTRING = new_String(AS_BOOL(value->value) <= 1 ? "true":"false");
                return new;
            case AST_CHAR:
                NEW_STRING;NSTRING = new_String(&value->value.as.charValue);
                return new;
            case AST_STRING: return value;
            case AST_NUMBER : 
                sprintf(str,"%g",AS_NUMBER(value->value));
                NEW_STRING;NSTRING = new_String(str);   
                return new;
            
        }
        #undef NEW_STRING
        #undef NSTRING

    }


    if(to == AST_LIST){
        if(value->type == AST_LIST)
            return value;

        ast_t *list = newAst(AST_LIST);
        newValue(&list->value,LIST);
        AS_LIST(list->value) = __init__list__(sizeof(ast_t*));
        List_Append(
            AS_LIST(list->value),
            value
        );
        return list;
    }

    return NULL;


}

ast_t *typer_typer(visitor_t *visitor ,const char *typer_name,list_ast_t *args){

        ast_t * new = NULL;
        int args_size = args->size;

    if(strcmp("int",typer_name) == 0 || 
    strcmp("float",typer_name) == 0){
        if(args_size > 1)return NULL;

        if(args_size == 0)
            return newAst(AST_NUMBER);
        
        new = typer_caste_value(AST_NUMBER,args->list[0]);

        AS_NUMBER(new->value) = strcmp("float",typer_name) != 0 ? 
        (int)AS_NUMBER(new->value):AS_NUMBER(new->value); 

        return new;
    }

    if(strcmp("char",typer_name) == 0){

        if(args_size > 1)return NULL;

        if(args_size == 0)
            return newAst(AST_CHAR);
        
        new = typer_caste_value(AST_CHAR,args->list[0]);
        return new;
    }

    if(strcmp("bool",typer_name) == 0){

        if(args_size > 1)return NULL;

        if(args_size == 0)
            return newAst(AST_BOOL);
        
        new = typer_caste_value(AST_BOOL,args->list[0]);
        return new;
    }

    if(strcmp("str",typer_name) == 0){
        // essaie prochainement de faire une mise à jour sur la concatenation
        if(args_size > 1)return NULL;

        if(args_size == 0)
            return newAst(AST_STRING);
        
        new = typer_caste_value(AST_STRING,args->list[0]);
        return new;
    }
    
    if(strcmp("list",typer_name) == 0){

        new = newAst(AST_LIST);
        new->value.as.listValue = __init__list__(sizeof(ast_t*));

        for (int i = 0; i < (int)args->size; i++){
            List_Append(
                AS_LIST(new->value),
                args->list[i] // a revoir !
            );
        }

        return new;
    }
    else{
        ast_t *func = newAst(AST_STRUCT);
        allowChar(&func->function_call_name,typer_name);
        func->function_call_args = *args;

        return visitor_visit(
            visitor,
            func
        );
    }

}