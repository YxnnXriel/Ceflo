#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "include/Value.h"
#include "include/typer.h"
#include "include/ast.h"


struct typer {
    uint8_t type;
    list_t *list;
    Obj_t *obj;
    string_t *str;
    ast_t * lfunc;
};


#define TYPE_TABLE_PRIORITY_COUNT 7

static int type_table_priority[TYPE_TABLE_PRIORITY_COUNT] = 
                    {AST_BOOL, AST_CHAR , AST_NUMBER,AST_STRING,
                    AST_LIST,AST_OBJ,AST_NONE};

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
        free_obj(type.obj);
    }
    if(type.type == AST_LFUNC){
        freeSingleAst(type.lfunc);
    }
}


static bool estToutNombre(const char *chaine) {
    if (chaine == NULL || *chaine == '\0') {
        return false;
    }

    while (*chaine) {
        if (!isdigit(*chaine)) {
            if(*chaine == '_' || *chaine == '.'){
                chaine++;
                continue;
            }else{
                return false;
            }

        }
        chaine++;
    }

    return true;
}

double convertirEnDouble(const char *chaine) {
    if (chaine == NULL || *chaine == '\0') {
        return 0.0;
    }

    // Variables pour stocker le résultat et le signe
    double resultat = 0.0;
    double decimale = 0.0;
    bool estNegatif = false;
    bool separateurDecimaleTrouve = false;

    // Parcourt la chaîne caractère par caractère
    while (*chaine) {
        if (*chaine == '_') {
            // Ignorer le caractère underscore
            chaine++;
            continue;
        }
        if (*chaine == '-') {
            // Gérer le signe négatif
            estNegatif = true;
        } else if (*chaine == '.') {
            // Gérer la partie décimale
            if (separateurDecimaleTrouve) {
                // Si un séparateur décimal a déjà été trouvé, la chaîne est invalide
                return 0.0;
            }
            separateurDecimaleTrouve = true;
        } else if (*chaine >= '0' && *chaine <= '9') {
            // Convertir les chiffres en nombre
            if (separateurDecimaleTrouve) {
                // Si c'est la partie décimale, ajouter au résultat décimal
                decimale = decimale * 10 + (*chaine - '0');
            } else {
                // Si c'est la partie entière, ajouter au résultat
                resultat = resultat * 10 + (*chaine - '0');
            }
        } else {
            // Si un caractère non numérique est rencontré, la chaîne est invalide
            return 0.0;
        }
        chaine++;
    }

    // Appliquer le signe négatif si nécessaire
    if (estNegatif) {
        resultat = -resultat;
        decimale = -decimale;
    }

    // Ajouter la partie décimale au résultat
    resultat += decimale / 10;

    return resultat;
}


ast_t * typer_caste_value_try_obj( uint8_t to , ast_t **value , ast_t *obj){

    if(to == AST_OBJ){
        if((*value)->type == AST_OBJ){
            if(strcmp((*value)->value.as.ObjectValue.obj_name , obj->variable_definition_static_type_name) == 0){
                return *value;
            }
        }
    }
    return NULL;
}


ast_t * typer_caste_value( uint8_t to , ast_t **value){


    char str[50];
    ast_t *new = *value;


    if(to == AST_BOOL){
        new->value.type = to - 4;

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
        new->value.type = to - 4;
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
        new->value.type = to - 4;
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
                NEW_NUMBER;
                if(estToutNombre(AS_STRING((*value)->value))){
                    NNUMBER = convertirEnDouble(AS_STRING((*value)->value));
                }else{
                    NNUMBER = strlen(AS_STRING((*value)->value));
                }
                save_free();
                return new;
            
        }
        #undef NEW_NUMBER
        #undef NNUMBER

    }

    if(to == AST_STRING){
        new->value.type = to - 4;
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
        new->value.type = to - 4;
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
        ast_t * args1 = visitor_visit(visitor,List_Ast_Get(args,0)); 

    if(strcmp("int",typer_name) == 0 || 
    strcmp("float",typer_name) == 0){
        if(args_size > 1){
            return NULL;
        }

        if(args_size == 0)
            return newAst(AST_NUMBER);
        
        new = typer_caste_value(AST_NUMBER,&args1);

        AS_NUMBER(new->value) = strcmp("float",typer_name) != 0 ? 
        (int)AS_NUMBER(new->value):AS_NUMBER(new->value); 

        return new;
    }

    if(strcmp("char",typer_name) == 0){

        if(args_size > 1)return NULL;

        if(args_size == 0)
            return newAst(AST_CHAR);
        
        new = typer_caste_value(AST_CHAR,&args1);
        return new;
    }

    if(strcmp("bool",typer_name) == 0){

        if(args_size > 1)return NULL;

        if(args_size == 0)
            return newAst(AST_BOOL);
        
        new = typer_caste_value(AST_BOOL,&args1);
        return new;
    }

    if(strcmp("str",typer_name) == 0){
        // essaie prochainement de faire une mise à jour sur la concatenation
        if(args_size > 1)return NULL;

        if(args_size == 0)
            return newAst(AST_STRING);
        
        new = typer_caste_value(AST_STRING,&args1);
        return new;
    }
    
    if(strcmp("list",typer_name) == 0){

        new = newAst(AST_LIST);
        new->value.as.listValue = __init__list__(sizeof(ast_t*));

        for (int i = 0; i < (int)args->size; i++){
            List_Append(
                AS_LIST(new->value),
                // args->list[i] 
                visitor_visit(visitor,List_Ast_Get(args,i))
                // a revoir !
            );
        }

        return new;
    }
    else{

        /**
         * TODO: rechercher dans les structures et trouver ça réprésentation
         * 
         */
        ast_t *func = newAst(AST_STRUCT);
        allowChar(&func->function_call_name,typer_name);
        func->function_call_args = List_Ast_Cpy(args);

        return visitor_visit(
            visitor,
            func
        );
    }

}

/*
    bool - > char -> number -> string -> list -> obj  
*/


static int getpriority(uint8_t type){
    for (int i = 0; i < TYPE_TABLE_PRIORITY_COUNT; i++){
        if(type_table_priority[i] == type)
            return i;
    }
    return -1;
}


ast_t * typer_value_expr(ast_t * left , token_t * op , ast_t * right){
    // cette fonction vas permettre de verifer qui à la priorité et faire un caste 
    // sur la valeur à le faire


    // ast_t ** priority_at = getpriority(left->type) >= getpriority(right->type) ? &left:&right;
    // // ast_t ** unpriority_at = getpriority(left) >= getpriority(right) ? &right:&left;
    
    // ast_t * value = newAst((*priority_at)->type);

    // switch (op->type){
    //     case TOKEN_PLUS:
    //         add_value(&left->value,&right->value,&value->value);
    //         return value;
    //     break;
        
    //     case TOKEN_MINUS:
    //         minus_value(&left->value,&right->value,&value->value);

    //         if(!Value_verif_is_none(&value->value))
    //             value->type = AST_NONE; // appeler les erreurs ici , MinusExprErreur


    //         return value;
    //     break;
    
    // }


    // return left;

    
}

