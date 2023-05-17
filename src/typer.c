#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "include/typer.h"
#include "include/ast.h"


struct typer {
    uint8_t type;
    list_t *list;
    Obj_t *obj;
    string_t *str;
    ast_t * lfunc;
};

struct typer type;

static void typer_verif(ast_t * val){
    if(val->type == AST_LIST){
        type.type = val->type;
        type.list = AS_LIST(val->value);
    }
    if(val->type == AST_STRING){
        type.type = val->type;
        type.str = val->value.as.stringValue;
    }
    if(val->type == AST_OBJ){
        type.type = val->type;
        type.obj = &val->value.as.ObjectValue;
    }
    if(val->type == AST_LFUNC){
        type.type = val->type;
        type.lfunc = val->value.as.lfuncValue;
    }
}

static void save_free(){

    if(type.type == AST_LIST){
        free(type.list->Item);
        free(type.list);
    }
    if(type.type == AST_STRING){
        String_free(type.str);
    }

    if(type.type == AST_OBJ){
        if(type.obj->obj_name != NULL)
            free(type.obj->obj_name);

        freeScope(type.obj->self);
    }
    if(type.type == AST_LFUNC){
        freeSingleAst(type.lfunc);
    }
}


ast_t * typer_caste_value( uint8_t to , ast_t **value){


    char str[50];
    ast_t *new = *value;


    if(to == AST_BOOL){
        #define NEW_BOOL new->type = AST_BOOL
        #define NBOOL new->value.as.booleanValue 
        switch ((*value)->type){
            case AST_BOOL:return *value;
            case AST_CHAR:
                typer_verif(*value);
                NEW_BOOL;NBOOL = AS_CHAR((*value)->value);
                save_free();
                return new;
            case AST_NUMBER: 
                typer_verif(*value);
                NEW_BOOL;NBOOL = AS_NUMBER((*value)->value);
                save_free();
                return new;
            case AST_STRING :
                typer_verif(*value);
                NEW_BOOL;NBOOL = strlen(AS_STRING((*value)->value));
                save_free();
                return new;
        }
        #undef NEW_BOOL
        #undef NBOOL
    }

    if(to == AST_CHAR){
        #define NEW_CHAR new->type = AST_CHAR
        #define NCHAR new->value.as.charValue 
        switch ((*value)->type){
            case AST_BOOL:
                typer_verif(*value);
                NEW_CHAR;NCHAR = AS_BOOL((*value)->value);
                save_free();
                return new;
            case AST_CHAR: return *value;
            case AST_NUMBER:
                typer_verif(*value);
                NEW_CHAR;NCHAR = AS_NUMBER((*value)->value);
                save_free();
                return new;
            case AST_STRING : 
                typer_verif(*value);
                NEW_CHAR;NCHAR = strlen(AS_STRING((*value)->value));
                save_free();
                return new;
            
        }
        #undef NEW_CHAR
        #undef NCHAR

    }
    
    if(to == AST_NUMBER){
        #define NEW_NUMBER new->type = AST_NUMBER
        #define NNUMBER new->value.as.numberValue 
        switch ((*value)->type){
            case AST_BOOL:
                typer_verif(*value);
                NEW_NUMBER;NNUMBER = AS_BOOL((*value)->value);
                save_free();
                return new;
            case AST_CHAR:
                typer_verif(*value);
                NEW_NUMBER;NNUMBER = AS_CHAR((*value)->value);
                save_free();
                return new;
            case AST_NUMBER: return *value;
            case AST_STRING : 
                typer_verif(*value);
                NEW_NUMBER;NNUMBER = strlen(AS_STRING((*value)->value));
                save_free();
                return new;
            
        }
        #undef NEW_NUMBER
        #undef NNUMBER

    }

    if(to == AST_STRING){
        #define NEW_STRING new->type = AST_STRING
        #define NSTRING new->value.as.stringValue 
        switch ((*value)->type){
            case AST_BOOL:
                typer_verif(*value);
                NEW_STRING;NSTRING = new_String(AS_BOOL((*value)->value) <= 1 ? "true":"false");
                save_free();
                return new;
            case AST_CHAR:
                typer_verif(*value);
                NEW_STRING;NSTRING = new_String(&(*value)->value.as.charValue);
                save_free();
                return new;
            case AST_STRING: return *value;
            case AST_NUMBER : 
                typer_verif(*value);
                sprintf(str,"%g",AS_NUMBER((*value)->value));
                NEW_STRING;NSTRING = new_String(str);   
                save_free();
                return new;
            
        }
        #undef NEW_STRING
        #undef NSTRING

    }


    if(to == AST_LIST){
        if((*value)->type == AST_LIST)
            return *value;

        ast_t *list = newAst(AST_LIST);
        newValue(&list->value,LIST);
        AS_LIST(list->value) = __init__list__(sizeof(ast_t*));
        List_Append(
            AS_LIST(list->value),
            *value
        );

        typer_verif(*value);
        (*value)->value.as.listValue = AS_LIST(list->value);
        save_free();
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
        
        new = typer_caste_value(AST_NUMBER,&args->list[0]);

        AS_NUMBER(new->value) = strcmp("float",typer_name) != 0 ? 
        (int)AS_NUMBER(new->value):AS_NUMBER(new->value); 

        return new;
    }

    if(strcmp("char",typer_name) == 0){

        if(args_size > 1)return NULL;

        if(args_size == 0)
            return newAst(AST_CHAR);
        
        new = typer_caste_value(AST_CHAR,&args->list[0]);
        return new;
    }

    if(strcmp("bool",typer_name) == 0){

        if(args_size > 1)return NULL;

        if(args_size == 0)
            return newAst(AST_BOOL);
        
        new = typer_caste_value(AST_BOOL,&args->list[0]);
        return new;
    }

    if(strcmp("str",typer_name) == 0){
        // essaie prochainement de faire une mise à jour sur la concatenation
        if(args_size > 1)return NULL;

        if(args_size == 0)
            return newAst(AST_STRING);
        
        new = typer_caste_value(AST_STRING,&args->list[0]);
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