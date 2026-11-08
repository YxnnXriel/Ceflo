#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "ceflo.h"

/*
    Les types écrits avec `::` et les conversions `@type(x)`. Les règles sont
    celles de src/typer.c.
*/

TypeKind typer_kind(const char *type_name){
    if(type_name == NULL || type_name[0] == '\0')
        return TYPE_ANY;
    if(strcmp(type_name,"int") == 0)
        return TYPE_INT;
    if(strcmp(type_name,"float") == 0 || strcmp(type_name,"number") == 0)
        return TYPE_FLOAT;
    if(strcmp(type_name,"bool") == 0)
        return TYPE_BOOL;
    if(strcmp(type_name,"char") == 0)
        return TYPE_CHAR;
    if(strcmp(type_name,"str") == 0 || strcmp(type_name,"string") == 0)
        return TYPE_STR;
    if(strcmp(type_name,"list") == 0)
        return TYPE_LIST;
    if(strcmp(type_name,"map") == 0)
        return TYPE_MAP;
    return TYPE_NAMED;
}

// Une structure ou une énumération du script : une globale du fichier, sinon un builtin.
static Value typer_find_type(VM *vm, ObjModule *module, ObjString *name){
    Value found = UNDEFINED_VAL;
    Value index;
    if(module != NULL && table_get(&module->indexes,name,&index))
        found = module->globals.values[(int)AS_NUMBER(index)];
    if(IS_UNDEFINED(found) && !table_get(&vm->builtins,name,&found))
        return UNDEFINED_VAL;
    return IS_STRUCT(found) || IS_ENUM(found) ? found : UNDEFINED_VAL;
}

static Value typer_need_type(VM *vm, ObjModule *module, ObjString *name){
    Value found = typer_find_type(vm,module,name);
    if(IS_UNDEFINED(found))
        vm_error(vm,"le type `%s` n'existe pas",name->chars);
    return found;
}

static bool typer_is_whole(Value value){
    return IS_NUMBER(value) && AS_NUMBER(value) == trunc(AS_NUMBER(value));
}

bool typer_check(VM *vm, ObjModule *module, Value value, TypeRef *type){
    switch(type->kind){
        case TYPE_ANY: return true;
        case TYPE_INT: return typer_is_whole(value);
        case TYPE_FLOAT: return IS_NUMBER(value);
        case TYPE_BOOL: return IS_BOOL(value);
        case TYPE_CHAR: return IS_CHAR(value);
        case TYPE_STR: return IS_STRING(value);
        case TYPE_LIST: return IS_LIST(value);
        case TYPE_MAP: return IS_MAP(value);
        case TYPE_NAMED: break;
    }

    Value found = typer_need_type(vm,module,type->name);

    // Un objet peut ne pas encore exister : `var Job::Metier` vaut none.
    if(IS_NONE(value))
        return true;
    if(IS_ENUM(found))
        return IS_ENUM_VALUE(value) && AS_OBJ(AS_ENUM_VALUE(value)->owner) == AS_OBJ(found);
    return IS_INSTANCE(value) && (Obj*)AS_INSTANCE(value)->structure == AS_OBJ(found);
}

Value typer_default(VM *vm, ObjModule *module, TypeRef *type){
    switch(type->kind){
        case TYPE_ANY: return NONE_VAL;
        case TYPE_INT:
        case TYPE_FLOAT: return NUMBER_VAL(0);
        case TYPE_BOOL: return BOOL_VAL(false);
        case TYPE_CHAR: return CHAR_VAL('\0');
        case TYPE_STR: return OBJ_VAL(string_copy(vm,"",0));
        case TYPE_LIST: return OBJ_VAL(list_new(vm));
        case TYPE_MAP: return OBJ_VAL(map_new(vm));
        case TYPE_NAMED: break;
    }
    typer_need_type(vm,module,type->name);
    return NONE_VAL;
}


//                          ===================================== $CONVERSIONS ============================================


/**
 * @brief lit le nombre écrit dans une chaîne. CeFlo donne sa longueur à une
 *        chaîne qui n'est pas un nombre : `@int("abc")` vaut 3.
 */
static double typer_string_to_number(ObjString *str){
    char *end = NULL;
    double number = strtod(str->chars,&end);

    while(end != NULL && isspace((unsigned char)*end))
        end++;

    if(str->length == 0 || end == str->chars || *end != '\0')
        return str->length;

    return number;
}

// Les conversions retournent UNDEFINED quand elles sont impossibles.
static Value typer_to_number(Value value){
    switch(value.type){
        case VAL_NUMBER: return value;
        case VAL_BOOL: return NUMBER_VAL(AS_BOOL(value) ? 1 : 0);
        case VAL_CHAR: return NUMBER_VAL((unsigned char)AS_CHAR(value));
        default: break;
    }
    if(IS_STRING(value))
        return NUMBER_VAL(typer_string_to_number(AS_STRING(value)));
    if(IS_ENUM_VALUE(value) && IS_NUMBER(AS_ENUM_VALUE(value)->value))
        return AS_ENUM_VALUE(value)->value;
    return UNDEFINED_VAL;
}

static Value typer_to_int(Value value){
    value = typer_to_number(value);
    return IS_NUMBER(value) ? NUMBER_VAL(trunc(AS_NUMBER(value))) : value;
}

static Value typer_to_char(Value value){
    switch(value.type){
        case VAL_CHAR: return value;
        case VAL_NUMBER:{
            double number = AS_NUMBER(value);
            return CHAR_VAL(isfinite(number) ? (char)(int)fmod(number,256) : '\0');
        }
        case VAL_BOOL: return CHAR_VAL((char)AS_BOOL(value));
        default: break;
    }
    if(IS_STRING(value))
        return CHAR_VAL(AS_CSTRING(value)[0]);
    return UNDEFINED_VAL;
}

static Value typer_to_list(VM *vm, Value value){
    if(IS_LIST(value))
        return value;
    ObjList *list = list_new(vm);
    list_append(vm,list,value);
    return OBJ_VAL(list);
}

// Vers une structure : on garde les objets du bon type et `none`, sinon on demande à __cast__.
static Value typer_to_struct(VM *vm, Value value, ObjStruct *structure){
    if(IS_NONE(value))
        return value;
    if(IS_INSTANCE(value) && AS_INSTANCE(value)->structure == structure)
        return value;
    if(IS_NONE(structure->cast_method))
        return UNDEFINED_VAL;

    // __cast__(x) n'a pas de self : s'il est compilé comme une méthode, self vaut none.
    Value cast = structure->cast_method;
    if(IS_CLOSURE(cast) && AS_CLOSURE(cast)->function->is_method)
        cast = OBJ_VAL(bound_method_new(vm,NONE_VAL,cast));

    // vm_call copie ses arguments, et la structure reste une globale : rien à protéger.
    Value result = vm_call(vm,cast,1,&value);
    if(!IS_INSTANCE(result) || AS_INSTANCE(result)->structure != structure)
        return UNDEFINED_VAL;
    return result;
}

// Retrouve l'élément qui a cette valeur, ou NULL.
static ObjEnumValue *typer_enum_find(ObjEnum *enumeration, Value value){
    for(int i = 0; i < enumeration->count; i++){
        Value item = enumeration->items[i]->value;
        if(IS_NUMBER(value) && IS_NUMBER(item) && AS_NUMBER(item) == AS_NUMBER(value))
            return enumeration->items[i];
        if(IS_STRING(value) && IS_STRING(item) && AS_STRING(item)->length == AS_STRING(value)->length &&
           memcmp(AS_CSTRING(item),AS_CSTRING(value),(size_t)AS_STRING(value)->length) == 0)
            return enumeration->items[i];
    }
    return NULL;
}

// Vers une énumération : on garde ses éléments et `none`, et une valeur inconnue donne `none`.
static Value typer_to_enum(Value value, ObjEnum *enumeration){
    if(IS_NONE(value))
        return value;
    if(IS_ENUM_VALUE(value))
        return AS_OBJ(AS_ENUM_VALUE(value)->owner) == (Obj*)enumeration ? value : UNDEFINED_VAL;
    if(!IS_NUMBER(value) && !IS_STRING(value))
        return UNDEFINED_VAL;

    ObjEnumValue *member = typer_enum_find(enumeration,value);
    return member != NULL ? OBJ_VAL(member) : NONE_VAL;
}

static Value typer_cast_value(VM *vm, ObjModule *module, Value value, ObjString *type_name){
    switch(typer_kind(type_name->chars)){
        case TYPE_INT: return typer_to_int(value);
        case TYPE_FLOAT: return typer_to_number(value);
        case TYPE_BOOL: return IS_BOOL(value) ? value : BOOL_VAL(value_is_true(value));
        case TYPE_CHAR: return typer_to_char(value);
        case TYPE_STR:
            if(IS_ENUM_VALUE(value))
                return OBJ_VAL(AS_ENUM_VALUE(value)->name);
            return IS_STRING(value) ? value : OBJ_VAL(vm_to_string(vm,value));
        case TYPE_LIST: return typer_to_list(vm,value);
        case TYPE_MAP: return IS_MAP(value) ? value : UNDEFINED_VAL;
        default: break;
    }

    Value found = typer_need_type(vm,module,type_name);
    if(IS_ENUM(found))
        return typer_to_enum(value,AS_ENUM(found));
    return typer_to_struct(vm,value,AS_STRUCT(found));
}

Value typer_type_call(VM *vm, ObjModule *module, ObjString *type_name, int argc, Value *args){
    TypeKind kind = typer_kind(type_name->chars);

    if(kind == TYPE_LIST){
        ObjList *list = list_new(vm);
        for(int i = 0; i < argc; i++)
            list_append(vm,list,args[i]);
        return OBJ_VAL(list);
    }

    // @Point(1, 2) : une structure se construit comme avec `new`, sauf si elle a un __cast__
    if(kind == TYPE_NAMED){
        Value structure = typer_find_type(vm,module,type_name);
        if(IS_STRUCT(structure) && (argc != 1 || IS_NONE(AS_STRUCT(structure)->cast_method)))
            return vm_new_instance(vm,AS_STRUCT(structure),argc,args);
    }

    if(argc == 0){
        TypeRef type = {kind,type_name};
        return typer_default(vm,module,&type);
    }

    native_check_args(vm,type_name->chars,argc,1);

    Value result = typer_cast_value(vm,module,args[0],type_name);
    if(IS_UNDEFINED(result))
        vm_error(vm,"impossible de convertir %s en %s",value_type_name(args[0]),type_name->chars);
    return result;
}
