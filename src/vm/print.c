#include <stdio.h>
#include <math.h>

#include "vm.h"
#include "common.h"
#include "str.h"

/*
    Le texte d'une valeur, exactement comme Value_to_buffer dans src/Value.c.
    Une liste peut se contenir elle-même : un conteneur déjà en cours
    d'affichage (obj.printing) s'écrit "...".
*/

static void print_value(VM *vm, buffer_t *buffer, Value value);

// Les entiers s'écrivent en entier : 1000000 et non 1e+06.
static void print_number(buffer_t *buffer, double number){
    char text[64];

    // Le cas le plus courant, sans passer par snprintf.
    if(number == floor(number) && fabs(number) < 1e15){
        char *end = text + sizeof(text);
        char *digit = end;
        long long whole = (long long)fabs(number);
        do{
            *--digit = (char)('0' + whole % 10);
            whole /= 10;
        }while(whole > 0);
        if(number < 0)
            *--digit = '-';
        Buffer_Append(buffer,digit,(size_t)(end - digit));
        return;
    }

    snprintf(text,sizeof(text),"%.15g",number);
    Buffer_Append_Str(buffer,text);
}

// Dans une liste, on distingue "1" de 1 : les chaînes gardent leurs guillemets.
static void print_item(VM *vm, buffer_t *buffer, Value item){
    if(IS_STRING(item)){
        Buffer_Append_Char(buffer,'"');
        Buffer_Append(buffer,AS_CSTRING(item),(size_t)AS_STRING(item)->length);
        Buffer_Append_Char(buffer,'"');
    }else if(IS_CHAR(item)){
        Buffer_Append_Char(buffer,'\'');
        Buffer_Append_Char(buffer,AS_CHAR(item));
        Buffer_Append_Char(buffer,'\'');
    }else{
        print_value(vm,buffer,item);
    }
}

static void print_list(VM *vm, buffer_t *buffer, ObjList *list){
    if(list->count == 0){
        Buffer_Append_Str(buffer,"[]");
        return;
    }

    Buffer_Append_Str(buffer,"[ ");
    // count est relu à chaque tour : un __str__ peut modifier la liste.
    for(int i = 0; i < list->count; i++){
        print_item(vm,buffer,list->items[i]);
        if(i != list->count - 1)
            Buffer_Append_Str(buffer," , ");
    }
    Buffer_Append_Str(buffer," ]");
}

static void print_map(VM *vm, buffer_t *buffer, ObjMap *map){
    if(map->count == 0){
        Buffer_Append_Str(buffer,"{}");
        return;
    }

    bool first = true;
    Buffer_Append_Str(buffer,"{ ");
    for(int i = 0; i < map->size; i++){
        if(IS_UNDEFINED(map->entries[i].key))
            continue;
        if(!first)
            Buffer_Append_Str(buffer," , ");
        first = false;
        print_item(vm,buffer,map->entries[i].key);
        Buffer_Append_Str(buffer," : ");
        print_item(vm,buffer,map->entries[i].value);
    }
    Buffer_Append_Str(buffer," }");
}

/*
    __str__ est du code CeFlo : il passe par des points sûrs. La méthode liée
    et son résultat restent donc sur la pile de la VM pendant qu'on s'en sert.
*/
static void print_instance(VM *vm, buffer_t *buffer, Value value){
    ObjInstance *instance = AS_INSTANCE(value);

    if(IS_NONE(instance->structure->str_method)){
        Buffer_Append_Char(buffer,'<');
        Buffer_Append_Str(buffer,instance->structure->name->chars);
        Buffer_Append_Char(buffer,'>');
        return;
    }

    Value method = OBJ_VAL(bound_method_new(vm,value,instance->structure->str_method));
    vm_push(vm,method);
    Value result = vm_call(vm,method,0,NULL);
    vm_pop(vm);

    vm_push(vm,result);
    print_value(vm,buffer,result);
    vm_pop(vm);
}

static void print_container(VM *vm, buffer_t *buffer, Value value){
    Obj *object = AS_OBJ(value);
    if(object->printing){
        Buffer_Append_Str(buffer,"...");
        return;
    }
    if(ceflo_stack_exceeded())
        vm_error(vm,"valeur trop profonde pour être affichée");

    object->printing = true;
    if(IS_LIST(value))
        print_list(vm,buffer,AS_LIST(value));
    else if(IS_MAP(value))
        print_map(vm,buffer,AS_MAP(value));
    else
        print_instance(vm,buffer,value);
    object->printing = false;
}

static void print_func(buffer_t *buffer, const char *name){
    Buffer_Append_Str(buffer,"<func ");
    // Le code d'un fichier n'a pas de nom ; une fonction anonyme s'appelait lfunc.
    Buffer_Append_Str(buffer,name != NULL ? name : "lfunc");
    Buffer_Append_Char(buffer,'>');
}

static const char *print_function_name(Value function){
    if(IS_NATIVE(function))
        return AS_NATIVE(function)->name;
    ObjString *name = IS_CLOSURE(function) ? AS_CLOSURE(function)->function->name : AS_FUNCTION(function)->name;
    return name != NULL ? name->chars : NULL;
}

static void print_object(VM *vm, buffer_t *buffer, Value value){
    switch(OBJ_TYPE(value)){
        case OBJ_STRING:
            Buffer_Append(buffer,AS_CSTRING(value),(size_t)AS_STRING(value)->length);
            break;
        case OBJ_LIST:
        case OBJ_MAP:
        case OBJ_INSTANCE:
            print_container(vm,buffer,value);
            break;
        case OBJ_FUNCTION:
        case OBJ_CLOSURE:
        case OBJ_NATIVE:
            print_func(buffer,print_function_name(value));
            break;
        case OBJ_BOUND_METHOD:
            print_func(buffer,print_function_name(AS_BOUND_METHOD(value)->method));
            break;
        case OBJ_STRUCT:
            Buffer_Append_Str(buffer,"<struct ");
            Buffer_Append_Str(buffer,AS_STRUCT(value)->name->chars);
            Buffer_Append_Char(buffer,'>');
            break;
        case OBJ_ENUM:
            Buffer_Append_Str(buffer,"<enum ");
            Buffer_Append_Str(buffer,AS_ENUM(value)->name->chars);
            Buffer_Append_Char(buffer,'>');
            break;
        case OBJ_ENUM_VALUE:
            Buffer_Append_Str(buffer,value_type_name(value));
            Buffer_Append_Char(buffer,'.');
            Buffer_Append_Str(buffer,AS_ENUM_VALUE(value)->name->chars);
            break;
        case OBJ_MODULE:
            Buffer_Append_Str(buffer,"<module ");
            Buffer_Append_Str(buffer,AS_MODULE(value)->path->chars);
            Buffer_Append_Char(buffer,'>');
            break;
        case OBJ_GENERIC:
            Buffer_Append_Str(buffer,"<generic>");
            break;
        case OBJ_UPVALUE:
            break;
    }
}

static void print_value(VM *vm, buffer_t *buffer, Value value){
    switch(value.type){
        case VAL_NONE:
        case VAL_UNDEFINED: Buffer_Append_Str(buffer,"none"); break;
        case VAL_BOOL: Buffer_Append_Str(buffer,AS_BOOL(value) ? "true" : "false"); break;
        case VAL_CHAR: Buffer_Append_Char(buffer,AS_CHAR(value)); break;
        case VAL_NUMBER: print_number(buffer,AS_NUMBER(value)); break;
        case VAL_OBJ: print_object(vm,buffer,value); break;
    }
}

// La valeur reste sur la pile : un __str__ appelé en chemin ne doit pas la libérer.
void vm_value_to_buffer(VM *vm, buffer_t *buffer, Value value){
    vm_push(vm,value);
    print_value(vm,buffer,value);
    vm_pop(vm);
}

ObjString *vm_to_string(VM *vm, Value value){
    if(IS_STRING(value))
        return AS_STRING(value);

    // Le cas le plus courant : un nombre, sans tampon alloué.
    if(IS_NUMBER(value)){
        char text[64];
        buffer_t buffer = {text,0,sizeof(text)};
        print_number(&buffer,AS_NUMBER(value));
        return string_new(vm,text,(int)buffer.size);
    }

    buffer_t buffer;
    newBuffer(&buffer);
    vm_value_to_buffer(vm,&buffer,value);
    ObjString *string = string_new(vm,buffer.data,(int)buffer.size);
    Buffer_Free(&buffer);
    return string;
}
