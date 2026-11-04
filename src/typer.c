#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "include/typer.h"
#include "include/error.h"

/**
 * @brief lit le nombre écrit dans une chaîne. CeFlo donne sa longueur à une
 *        chaîne qui n'est pas un nombre : `@int("abc")` vaut 3.
 */
static double typer_string_to_number(string_t *str){
    char *end = NULL;
    double number = strtod(str->str,&end);

    while(end != NULL && isspace((unsigned char)*end))
        end++;

    if(str->lenght == 0 || end == str->str || *end != '\0')
        return str->lenght;

    return number;
}

static value_t *typer_to_number(value_t *value){
    switch(value->type){
        case NUMBER: return value;
        case BOOL:
        case CHAR: return makeNumber(Value_as_double(value));
        case STRING: return makeNumber(typer_string_to_number(AS_STRING(value)));
        default: return NULL;
    }
}

static value_t *typer_to_int(value_t *value){
    value = typer_to_number(value);
    return value != NULL ? makeNumber(trunc(AS_NUMBER(value))) : NULL;
}

static value_t *typer_to_char(value_t *value){
    switch(value->type){
        case CHAR: return value;
        case NUMBER: return makeChar(Value_number_to_char(AS_NUMBER(value)));
        case BOOL: return makeChar((char)AS_BOOL(value));
        case STRING: return makeChar(AS_STRING(value)->str[0]);
        default: return NULL;
    }
}

static value_t *typer_to_list(value_t *value){
    if(value->type == LIST)
        return value;
    value_t *list = makeList();
    Value_list_append(list,value);
    return list;
}

// Vers une structure : on garde les objets du bon type et `none`, sinon on demande à __cast__.
static value_t *typer_to_struct(visitor_t *visitor, ast_t *node, value_t *value, value_t *structure){
    if(value->type == NONE)
        return value;
    if(value->type == OBJECT && value->as.ObjectValue.structure == structure)
        return value;

    ast_t *cast = GetFunctionInStruct(structure->as.structValue.definition,"__cast__");
    if(cast == NULL)
        return NULL;

    value_t *function = makeFunction(cast,structure->as.structValue.closure);
    value_t *result = visitor_call(visitor,node,function,&value,1);

    if(result->type != OBJECT || result->as.ObjectValue.structure != structure)
        return NULL;
    return result;
}

static value_t *typer_find_struct(visitor_t *visitor, const char *type_name){
    hash_t *item = scope_lookup(visitor->scope,type_name);
    if(item == NULL || ((value_t*)item->value)->type != STRUCT)
        return NULL;
    return item->value;
}

static value_t *typer_caste_value(visitor_t *visitor, ast_t *node, value_t *value, const char *type_name){
    if(strcmp(type_name,"int") == 0)
        return typer_to_int(value);
    if(strcmp(type_name,"float") == 0 || strcmp(type_name,"number") == 0)
        return typer_to_number(value);
    if(strcmp(type_name,"bool") == 0)
        return value->type == BOOL ? value : makeBool(Value_is_true(value));
    if(strcmp(type_name,"char") == 0)
        return typer_to_char(value);
    if(strcmp(type_name,"str") == 0 || strcmp(type_name,"string") == 0)
        return Value_to_string(visitor,value);
    if(strcmp(type_name,"list") == 0)
        return typer_to_list(value);
    if(strcmp(type_name,"map") == 0)
        return value->type == MAP ? value : NULL;

    value_t *structure = typer_find_struct(visitor,type_name);
    if(structure == NULL)
        visitor_error(node,"le type `%s` n'existe pas",type_name);

    return typer_to_struct(visitor,node,value,structure);
}

static bool typer_is_whole(value_t *value){
    return value->type == NUMBER && AS_NUMBER(value) == trunc(AS_NUMBER(value));
}

bool typer_has_type(visitor_t *visitor, ast_t *node, value_t *value, const char *type_name){
    if(strcmp(type_name,"int") == 0)
        return typer_is_whole(value);
    if(strcmp(type_name,"float") == 0 || strcmp(type_name,"number") == 0)
        return value->type == NUMBER;
    if(strcmp(type_name,"bool") == 0)
        return value->type == BOOL;
    if(strcmp(type_name,"char") == 0)
        return value->type == CHAR;
    if(strcmp(type_name,"str") == 0 || strcmp(type_name,"string") == 0)
        return value->type == STRING;
    if(strcmp(type_name,"list") == 0)
        return value->type == LIST;
    if(strcmp(type_name,"map") == 0)
        return value->type == MAP;

    value_t *structure = typer_find_struct(visitor,type_name);
    if(structure == NULL)
        visitor_error(node,"le type `%s` n'existe pas",type_name);

    // Un objet peut ne pas encore exister : `var Job::Metier` vaut none.
    return value->type == NONE || (value->type == OBJECT && value->as.ObjectValue.structure == structure);
}

const char *typer_type_name(value_t *value){
    if(value->type == NUMBER)
        return typer_is_whole(value) ? "int" : "float";
    return Value_type_name(value);
}

value_t *typer_default_value(visitor_t *visitor, ast_t *node, const char *type_name){
    if(strcmp(type_name,"int") == 0 || strcmp(type_name,"float") == 0 || strcmp(type_name,"number") == 0)
        return makeNumber(0);
    if(strcmp(type_name,"bool") == 0)
        return makeBool(false);
    if(strcmp(type_name,"char") == 0)
        return makeChar('\0');
    if(strcmp(type_name,"str") == 0 || strcmp(type_name,"string") == 0)
        return makeString("");
    if(strcmp(type_name,"list") == 0)
        return makeList();
    if(strcmp(type_name,"map") == 0)
        return makeMap();

    if(typer_find_struct(visitor,type_name) == NULL)
        visitor_error(node,"le type `%s` n'existe pas",type_name);

    return makeNone();
}

value_t *typer_typer(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    const char *type_name = node->function_call_name;

    if(strcmp(type_name,"list") == 0){
        value_t *list = makeList();
        for(int i = 0; i < argc; i++)
            Value_list_append(list,args[i]);
        return list;
    }

    // @Point(1, 2) : une structure se construit comme avec `new`, sauf si elle a un __cast__
    value_t *structure = typer_find_struct(visitor,type_name);
    if(structure != NULL && (argc != 1 || GetFunctionInStruct(structure->as.structValue.definition,"__cast__") == NULL))
        return visitor_new_object(visitor,node,structure,args,argc);

    if(argc == 0)
        return typer_default_value(visitor,node,type_name);

    Value_check_args(node,type_name,argc,1);

    value_t *result = typer_caste_value(visitor,node,args[0],type_name);
    if(result == NULL)
        visitor_error(node,"impossible de convertir %s en %s",Value_type_name(args[0]),type_name);

    return result;
}
