#include <string.h>

#include "memory.h"

#define ALLOCATE_OBJ(vm,type,obj_type) \
    ((type*)vm_allocate_object(vm,sizeof(type),obj_type))

//                          ===================================== $CHAINES ==============================================


// FNV-1a : simple, et assez bon pour des noms et des textes courts.
uint32_t string_hash(const char *chars, int length){
    uint32_t hash = 2166136261u;
    for(int i = 0; i < length; i++)
        hash = (hash ^ (unsigned char)chars[i]) * 16777619u;
    return hash;
}

// Un texte plus long n'est pas interné : le hacher et le chercher coûterait
// autant que le construire, et `s += "ab"` dans une boucle deviendrait très lent.
#define STRING_INTERN_MAX 32

static ObjString *string_allocate(VM *vm, int length, uint32_t hash, bool interned){
    ObjString *string = (ObjString*)vm_allocate_object(vm,sizeof(ObjString) + (size_t)length + 1,OBJ_STRING);
    string->length = length;
    string->hash = hash;
    string->interned = interned;
    string->chars[length] = '\0';
    return string;
}

ObjString *string_copy(VM *vm, const char *chars, int length){
    uint32_t hash = string_hash(chars,length);
    ObjString *interned = table_find_string(&vm->strings,chars,length,hash);
    if(interned != NULL)
        return interned;

    ObjString *string = string_allocate(vm,length,hash,true);
    memcpy(string->chars,chars,(size_t)length);
    table_set(&vm->strings,string,NONE_VAL);
    return string;
}

ObjString *string_from(VM *vm, const char *chars){
    return string_copy(vm,chars,(int)strlen(chars));
}

ObjString *string_new(VM *vm, const char *chars, int length){
    if(length <= STRING_INTERN_MAX)
        return string_copy(vm,chars,length);

    ObjString *string = string_allocate(vm,length,0,false);
    memcpy(string->chars,chars,(size_t)length);
    return string;
}

ObjString *string_reserve(VM *vm, int length){
    return string_allocate(vm,length,0,false);
}

// 0 veut dire "pas encore calculé" : un hash qui vaut vraiment 0 est simplement recalculé.
uint32_t string_get_hash(ObjString *string){
    if(string->hash == 0)
        string->hash = string_hash(string->chars,string->length);
    return string->hash;
}

bool string_equals(ObjString *a, ObjString *b){
    if(a == b)
        return true;
    // Deux chaînes internées différentes n'ont jamais le même texte.
    if(a->length != b->length || (a->interned && b->interned))
        return false;
    return memcmp(a->chars,b->chars,(size_t)a->length) == 0;
}


//                          ===================================== $LIST ==============================================


ObjList *list_new(VM *vm){
    ObjList *list = ALLOCATE_OBJ(vm,ObjList,OBJ_LIST);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

void list_append(VM *vm, ObjList *list, Value value){
    if(list->count == list->capacity){
        int capacity = GROW_CAPACITY(list->capacity);
        list->items = VM_GROW_ARRAY(vm,Value,list->items,list->capacity,capacity);
        list->capacity = capacity;
    }
    list->items[list->count] = value;
    list->count += 1;
}


//                          ===================================== $FONCTIONS ==============================================


ObjFunction *function_new(VM *vm, ObjModule *module){
    ObjFunction *function = ALLOCATE_OBJ(vm,ObjFunction,OBJ_FUNCTION);
    function->arity = 0;
    function->upvalue_count = 0;
    chunk_init(&function->chunk);
    function->name = NULL;
    function->module = module;
    function->definition = NULL;
    function->param_types = NULL;
    function->return_type = (TypeRef){TYPE_ANY,NULL};
    function->is_method = false;
    return function;
}

ObjClosure *closure_new(VM *vm, ObjFunction *function){
    ObjUpvalue **upvalues = NULL;
    if(function->upvalue_count > 0){
        upvalues = VM_ALLOCATE(vm,ObjUpvalue*,function->upvalue_count);
        for(int i = 0; i < function->upvalue_count; i++)
            upvalues[i] = NULL;
    }

    ObjClosure *closure = ALLOCATE_OBJ(vm,ObjClosure,OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;
    return closure;
}

ObjUpvalue *upvalue_new(VM *vm, Value *slot){
    ObjUpvalue *upvalue = ALLOCATE_OBJ(vm,ObjUpvalue,OBJ_UPVALUE);
    upvalue->location = slot;
    upvalue->closed = NONE_VAL;
    upvalue->next = NULL;
    return upvalue;
}

ObjNative *native_new(VM *vm, const char *name, NativeFn function){
    ObjNative *native = ALLOCATE_OBJ(vm,ObjNative,OBJ_NATIVE);
    native->function = function;
    native->name = name;
    return native;
}

ObjBoundMethod *bound_method_new(VM *vm, Value receiver, Value method){
    ObjBoundMethod *bound = ALLOCATE_OBJ(vm,ObjBoundMethod,OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}


//                          ===================================== $STRUCTURES ==============================================


ObjStruct *struct_new(VM *vm, ObjString *name, ast_t *definition, int field_count){
    ObjStruct *structure = ALLOCATE_OBJ(vm,ObjStruct,OBJ_STRUCT);
    structure->name = name;
    structure->definition = definition;
    structure->field_count = field_count;
    structure->field_names = NULL;
    structure->field_types = NULL;
    structure->field_is_const = NULL;
    if(field_count > 0){
        structure->field_names = VM_ALLOCATE(vm,ObjString*,field_count);
        structure->field_types = VM_ALLOCATE(vm,TypeRef,field_count);
        structure->field_is_const = VM_ALLOCATE(vm,bool,field_count);
        for(int i = 0; i < field_count; i++){
            structure->field_names[i] = NULL;
            structure->field_types[i] = (TypeRef){TYPE_ANY,NULL};
            structure->field_is_const[i] = false;
        }
    }
    structure->fields_init = NONE_VAL;
    table_init(&structure->methods);
    for(int i = 0; i < 8; i++)
        structure->inits[i] = NONE_VAL;
    structure->str_method = NONE_VAL;
    structure->cast_method = NONE_VAL;
    return structure;
}

// Les noms sont internés : comparer les adresses suffit.
int struct_field_index(ObjStruct *structure, ObjString *name){
    for(int i = 0; i < structure->field_count; i++){
        if(structure->field_names[i] == name)
            return i;
    }
    return -1;
}

ObjInstance *instance_new(VM *vm, ObjStruct *structure){
    size_t size = sizeof(ObjInstance) + sizeof(Value) * (size_t)structure->field_count;
    ObjInstance *instance = (ObjInstance*)vm_allocate_object(vm,size,OBJ_INSTANCE);
    instance->structure = structure;
    for(int i = 0; i < structure->field_count; i++)
        instance->fields[i] = NONE_VAL;
    return instance;
}


//                          ===================================== $ENUMERATIONS ==============================================


ObjEnum *enum_new(VM *vm, ObjString *name, ast_t *definition, int count){
    ObjEnum *enumeration = ALLOCATE_OBJ(vm,ObjEnum,OBJ_ENUM);
    enumeration->name = name;
    enumeration->definition = definition;
    enumeration->count = count;
    enumeration->items = NULL;
    if(count > 0){
        enumeration->items = VM_ALLOCATE(vm,ObjEnumValue*,count);
        for(int i = 0; i < count; i++)
            enumeration->items[i] = NULL;
    }
    return enumeration;
}

ObjEnumValue *enum_value_new(VM *vm, ObjEnum *owner, ObjString *name, Value value){
    ObjEnumValue *item = ALLOCATE_OBJ(vm,ObjEnumValue,OBJ_ENUM_VALUE);
    item->owner = OBJ_VAL(owner);
    item->name = name;
    item->value = value;
    return item;
}


//                          ===================================== $MODULES ==============================================


ObjModule *module_new(VM *vm, ObjString *path){
    ObjModule *module = ALLOCATE_OBJ(vm,ObjModule,OBJ_MODULE);
    module->path = path;
    value_array_init(&module->globals);
    module->names = NULL;
    module->types = NULL;
    module->is_const = NULL;
    module->capacity = 0;
    table_init(&module->indexes);
    module->exports = NULL;
    module->export_count = 0;
    module->loaded = false;
    return module;
}

int module_global_index(VM *vm, ObjModule *module, ObjString *name){
    Value index;
    if(table_get(&module->indexes,name,&index))
        return (int)AS_NUMBER(index);

    int count = module->globals.count;
    if(count == module->capacity){
        int capacity = GROW_CAPACITY(module->capacity);
        module->names = VM_GROW_ARRAY(vm,ObjString*,module->names,module->capacity,capacity);
        module->types = VM_GROW_ARRAY(vm,TypeRef,module->types,module->capacity,capacity);
        module->is_const = VM_GROW_ARRAY(vm,bool,module->is_const,module->capacity,capacity);
        module->capacity = capacity;
    }

    value_array_write(&module->globals,UNDEFINED_VAL);
    module->names[count] = name;
    module->types[count] = (TypeRef){TYPE_ANY,NULL};
    module->is_const[count] = false;
    table_set(&module->indexes,name,NUMBER_VAL(count));
    return count;
}


//                          ===================================== $DIVERS ==============================================


ObjGeneric *generic_new(VM *vm, void *pointer){
    ObjGeneric *generic = ALLOCATE_OBJ(vm,ObjGeneric,OBJ_GENERIC);
    generic->pointer = pointer;
    return generic;
}
