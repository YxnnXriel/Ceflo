#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "object.h"
#include "common.h"
#include "error.h"

//                          ===================================== $TABLEAU ==============================================


void value_array_init(ValueArray *array){
    array->values = NULL;
    array->count = 0;
    array->capacity = 0;
}

void value_array_write(ValueArray *array, Value value){
    if(array->count == array->capacity){
        array->capacity = array->capacity < 8 ? 8 : array->capacity * 2;
        array->values = ceflo_realloc(array->values,sizeof(Value) * (size_t)array->capacity);
    }
    array->values[array->count] = value;
    array->count += 1;
}

void value_array_free(ValueArray *array){
    free(array->values);
    value_array_init(array);
}


//                          ===================================== $QUESTIONS ==============================================


bool value_is_true(Value value){
    switch(value.type){
        case VAL_NONE: return false;
        case VAL_BOOL: return AS_BOOL(value);
        case VAL_CHAR: return AS_CHAR(value) != '\0';
        case VAL_NUMBER: return AS_NUMBER(value) != 0;
        case VAL_OBJ:
            switch(OBJ_TYPE(value)){
                case OBJ_STRING: return AS_STRING(value)->length > 0;
                case OBJ_LIST: return AS_LIST(value)->count > 0;
                case OBJ_MAP: return AS_MAP(value)->count > 0;
                default: return true;
            }
        default: return false;
    }
}

static bool value_is_numeric(Value value){
    return value.type == VAL_NUMBER || value.type == VAL_BOOL || value.type == VAL_CHAR;
}

static double value_as_double(Value value){
    switch(value.type){
        case VAL_NUMBER: return AS_NUMBER(value);
        case VAL_BOOL: return AS_BOOL(value) ? 1 : 0;
        case VAL_CHAR: return (unsigned char)AS_CHAR(value);
        default: return 0;
    }
}

// Un char et une str : le char compte comme une chaîne d'un seul octet.
static bool value_char_equals_string(char c, Value string){
    return IS_STRING(string) && AS_STRING(string)->length == 1 && AS_CSTRING(string)[0] == c;
}

static bool value_list_equals(ObjList *a, ObjList *b){
    if(a->count != b->count)
        return false;
    if(ceflo_stack_exceeded())
        visitor_error(NULL,"liste trop profonde pour être comparée");
    for(int i = 0; i < a->count; i++){
        if(!value_equals(a->items[i],b->items[i]))
            return false;
    }
    return true;
}

bool value_equals(Value a, Value b){
    // Le cas de loin le plus courant.
    if(a.type == VAL_NUMBER && b.type == VAL_NUMBER)
        return AS_NUMBER(a) == AS_NUMBER(b);

    if(value_is_numeric(a) && value_is_numeric(b))
        return value_as_double(a) == value_as_double(b);

    if(a.type == VAL_CHAR)
        return value_char_equals_string(AS_CHAR(a),b);
    if(b.type == VAL_CHAR)
        return value_char_equals_string(AS_CHAR(b),a);

    if(a.type != b.type)
        return false;
    if(a.type != VAL_OBJ)
        return true;    // none, undefined

    if(AS_OBJ(a) == AS_OBJ(b))
        return true;
    if(OBJ_TYPE(a) != OBJ_TYPE(b))
        return false;

    switch(OBJ_TYPE(a)){
        case OBJ_STRING: return string_equals(AS_STRING(a),AS_STRING(b));
        case OBJ_LIST: return value_list_equals(AS_LIST(a),AS_LIST(b));
        case OBJ_GENERIC: return AS_GENERIC(a)->pointer == AS_GENERIC(b)->pointer;
        default: return false;
    }
}

const char *value_type_name(Value value){
    switch(value.type){
        case VAL_NONE: return "none";
        case VAL_BOOL: return "bool";
        case VAL_CHAR: return "char";
        case VAL_NUMBER: return "number";
        case VAL_UNDEFINED: return "none";
        case VAL_OBJ: break;
    }

    switch(OBJ_TYPE(value)){
        case OBJ_STRING: return "str";
        case OBJ_LIST: return "list";
        case OBJ_MAP: return "map";
        case OBJ_FUNCTION:
        case OBJ_CLOSURE:
        case OBJ_NATIVE:
        case OBJ_BOUND_METHOD: return "func";
        case OBJ_STRUCT: return "struct";
        case OBJ_INSTANCE: return AS_INSTANCE(value)->structure->name->chars;
        case OBJ_ENUM: return "enum";
        case OBJ_ENUM_VALUE: return AS_ENUM(AS_ENUM_VALUE(value)->owner)->name->chars;
        case OBJ_MODULE: return "module";
        case OBJ_GENERIC: return "generic";
        case OBJ_UPVALUE: break;
    }
    return "?";
}

const char *value_typer_name(Value value){
    if(IS_NUMBER(value))
        return AS_NUMBER(value) == trunc(AS_NUMBER(value)) ? "int" : "float";
    return value_type_name(value);
}
