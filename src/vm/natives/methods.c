#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "ceflo.h"
#include "str.h"

/*
    Les méthodes de list, map, str et enum : `liste.append(x)`, `t.keys()`...
    Elles reçoivent l'objet (self) et leurs arguments, comme une native.
*/

typedef Value (*MethodFn)(VM *vm, Value self, int argc, Value *args);

typedef struct{
    const char *name;
    MethodFn function;
}method_t;

static int method_check_index(VM *vm, const char *name, Value index){
    native_check_type(vm,name,index,"number");
    if(AS_NUMBER(index) != floor(AS_NUMBER(index)))
        vm_error(vm,"`%s` attend un index entier",name);
    // Un index aussi grand n'existe jamais : on évite de le convertir en int.
    return fabs(AS_NUMBER(index)) < INT_MAX ? (int)AS_NUMBER(index) : -1;
}

static void method_check_key(VM *vm, Value key){
    if(!map_key_is_valid(key))
        vm_error(vm,"une clé de map doit être str, number ou un élément d'énumération, pas %s",value_type_name(key));
}


//                          ===================================== $LIST ===================================================


static Value method_list_lenght(VM *vm, Value self, int argc, Value *args){
    (void)args;
    native_check_args(vm,"lenght",argc,0);
    return NUMBER_VAL(AS_LIST(self)->count);
}

static Value method_list_append(VM *vm, Value self, int argc, Value *args){
    for(int i = 0; i < argc; i++)
        list_append(vm,AS_LIST(self),args[i]);
    return NONE_VAL;
}

static Value method_list_get(VM *vm, Value self, int argc, Value *args){
    native_check_args(vm,"get",argc,1);
    ObjList *list = AS_LIST(self);
    int index = method_check_index(vm,"get",args[0]);
    return index >= 0 && index < list->count ? list->items[index] : NONE_VAL;
}

static Value method_list_remove(VM *vm, Value self, int argc, Value *args){
    native_check_args(vm,"remove",argc,1);
    ObjList *list = AS_LIST(self);
    int index = method_check_index(vm,"remove",args[0]);
    if(index < 0 || index >= list->count)
        vm_error(vm,"`remove` : l'index %d n'existe pas (la liste contient %d éléments)",index,list->count);

    Value item = list->items[index];
    memmove(&list->items[index],&list->items[index + 1],(size_t)(list->count - index - 1) * sizeof(Value));
    list->count -= 1;
    return item;
}

static Value method_list_print(VM *vm, Value self, int argc, Value *args){
    (void)args;
    native_check_args(vm,"print",argc,0);

    buffer_t buffer;
    newBuffer(&buffer);
    vm_value_to_buffer(vm,&buffer,self);
    fwrite(buffer.data,1,buffer.size,stdout);
    Buffer_Free(&buffer);
    fputc('\n',stdout);
    return NONE_VAL;
}

static Value method_list_filter(VM *vm, Value self, int argc, Value *args){
    native_check_args(vm,"filter",argc,1);

    // La fonction appelée passe par des points sûrs du GC : tout ce qu'on garde est protégé.
    Value function = args[0];
    Value result = OBJ_VAL(list_new(vm));
    Value item = NONE_VAL;
    vm_protect(vm,&self);
    vm_protect(vm,&function);
    vm_protect(vm,&result);
    vm_protect(vm,&item);

    // Taille relue à chaque tour : la fonction peut modifier la liste.
    for(int i = 0; i < AS_LIST(self)->count; i++){
        item = AS_LIST(self)->items[i];
        if(value_is_true(vm_call(vm,function,1,&item)))
            list_append(vm,AS_LIST(result),item);
    }

    vm_unprotect(vm,4);
    return result;
}


//                          ===================================== $MAP ====================================================


static Value method_map_lenght(VM *vm, Value self, int argc, Value *args){
    (void)args;
    native_check_args(vm,"lenght",argc,0);
    return NUMBER_VAL(AS_MAP(self)->count);
}

static Value method_map_set(VM *vm, Value self, int argc, Value *args){
    native_check_args(vm,"set",argc,2);
    method_check_key(vm,args[0]);
    map_set(vm,AS_MAP(self),args[0],args[1]);
    return self;
}

// t.get(cle) comme t[cle], et t.get(cle, 0) donne 0 si la clé n'existe pas.
static Value method_map_get(VM *vm, Value self, int argc, Value *args){
    if(argc != 1 && argc != 2)
        vm_error(vm,"`get` prend 1 ou 2 arguments, mais en reçoit %d",argc);
    method_check_key(vm,args[0]);

    Value value;
    if(map_get(AS_MAP(self),args[0],&value))
        return value;
    return argc == 2 ? args[1] : NONE_VAL;
}

static Value method_map_has(VM *vm, Value self, int argc, Value *args){
    native_check_args(vm,"has",argc,1);
    method_check_key(vm,args[0]);
    Value value;
    return BOOL_VAL(map_get(AS_MAP(self),args[0],&value));
}

static Value method_map_remove(VM *vm, Value self, int argc, Value *args){
    native_check_args(vm,"remove",argc,1);
    method_check_key(vm,args[0]);

    Value value;
    if(!map_get(AS_MAP(self),args[0],&value))
        return NONE_VAL;
    map_remove(AS_MAP(self),args[0]);
    return value;
}

static Value method_map_keys(VM *vm, Value self, int argc, Value *args){
    (void)args;
    native_check_args(vm,"keys",argc,0);

    ObjMap *map = AS_MAP(self);
    ObjList *result = list_new(vm);
    for(int i = 0; i < map->size; i++){
        if(!IS_UNDEFINED(map->entries[i].key))
            list_append(vm,result,map->entries[i].key);
    }
    return OBJ_VAL(result);
}

static Value method_map_values(VM *vm, Value self, int argc, Value *args){
    (void)args;
    native_check_args(vm,"values",argc,0);

    ObjMap *map = AS_MAP(self);
    ObjList *result = list_new(vm);
    for(int i = 0; i < map->size; i++){
        if(!IS_UNDEFINED(map->entries[i].key))
            list_append(vm,result,map->entries[i].value);
    }
    return OBJ_VAL(result);
}


//                          ===================================== $ENUM ===================================================


// Couleur.values() : les éléments, dans l'ordre de la définition.
static Value method_enum_values(VM *vm, Value self, int argc, Value *args){
    (void)args;
    native_check_args(vm,"values",argc,0);

    ObjEnum *enumeration = AS_ENUM(self);
    ObjList *result = list_new(vm);
    for(int i = 0; i < enumeration->count; i++)
        list_append(vm,result,OBJ_VAL(enumeration->items[i]));
    return OBJ_VAL(result);
}


//                          ===================================== $STR ====================================================


static Value method_str_lenght(VM *vm, Value self, int argc, Value *args){
    (void)args;
    native_check_args(vm,"lenght",argc,0);
    return NUMBER_VAL(AS_STRING(self)->length);
}

// "{} a {} ans".format("Yann", 21)
static Value method_str_format(VM *vm, Value self, int argc, Value *args){
    ObjString *str = AS_STRING(self);
    int used = 0;

    buffer_t buffer;
    newBuffer(&buffer);

    for(int i = 0; i < str->length; i++){
        if(str->chars[i] == '{' && i + 1 < str->length && str->chars[i + 1] == '}'){
            if(used >= argc){
                Buffer_Free(&buffer);
                vm_error(vm,"`format` : il y a plus de {} que d'arguments (%d)",argc);
            }
            vm_value_to_buffer(vm,&buffer,args[used]);
            used += 1;
            i += 1;
        }else{
            Buffer_Append_Char(&buffer,str->chars[i]);
        }
    }

    ObjString *result = string_new(vm,buffer.data,(int)buffer.size);
    Buffer_Free(&buffer);
    return OBJ_VAL(result);
}

// "a,b".split(",") donne [ "a" , "b" ]
static Value method_str_split(VM *vm, Value self, int argc, Value *args){
    native_check_args(vm,"split",argc,1);
    native_check_type(vm,"split",args[0],"str");

    ObjString *str = AS_STRING(self);
    ObjString *delim = AS_STRING(args[0]);
    if(delim->length == 0)
        vm_error(vm,"`split` : le séparateur ne peut pas être vide");

    ObjList *result = list_new(vm);
    int start = 0;
    for(int i = 0; i + delim->length <= str->length; ){
        if(memcmp(str->chars + i,delim->chars,(size_t)delim->length) == 0){
            list_append(vm,result,OBJ_VAL(string_new(vm,str->chars + start,i - start)));
            i += delim->length;
            start = i;
        }else{
            i += 1;
        }
    }
    list_append(vm,result,OBJ_VAL(string_new(vm,str->chars + start,str->length - start)));
    return OBJ_VAL(result);
}


//                          ===================================== $METHODES ===============================================


static const method_t Method_List[] = {
    {"get",&method_list_get},
    {"remove",&method_list_remove},
    {"append",&method_list_append},
    {"lenght",&method_list_lenght},
    {"length",&method_list_lenght},
    {"print",&method_list_print},
    {"filter",&method_list_filter},
    {NULL,NULL},
};

static const method_t Method_Map[] = {
    {"get",&method_map_get},
    {"set",&method_map_set},
    {"has",&method_map_has},
    {"remove",&method_map_remove},
    {"keys",&method_map_keys},
    {"values",&method_map_values},
    {"lenght",&method_map_lenght},
    {"length",&method_map_lenght},
    {NULL,NULL},
};

static const method_t Method_Enum[] = {
    {"values",&method_enum_values},
    {NULL,NULL},
};

static const method_t Method_Str[] = {
    {"lenght",&method_str_lenght},
    {"length",&method_str_lenght},
    {"format",&method_str_format},
    {"split",&method_str_split},
    {NULL,NULL},
};

// Le hash de chaque nom, calculé une fois : une méthode se trouve sans strcmp.
#define METHODS_MAX 16

typedef struct{
    const method_t *methods;
    uint32_t hashes[METHODS_MAX];
    int lengths[METHODS_MAX];
    bool ready;
}method_table_t;

static method_table_t Tables[] = {
    {Method_List,{0},{0},false},
    {Method_Map,{0},{0},false},
    {Method_Enum,{0},{0},false},
    {Method_Str,{0},{0},false},
};

static const method_t *method_find(method_table_t *table, ObjString *name){
    if(!table->ready){
        for(int i = 0; table->methods[i].name != NULL; i++){
            table->lengths[i] = (int)strlen(table->methods[i].name);
            table->hashes[i] = string_hash(table->methods[i].name,table->lengths[i]);
        }
        table->ready = true;
    }

    uint32_t hash = string_get_hash(name);
    for(int i = 0; table->methods[i].name != NULL; i++){
        if(table->hashes[i] == hash && table->lengths[i] == name->length &&
           memcmp(table->methods[i].name,name->chars,(size_t)name->length) == 0)
            return &table->methods[i];
    }
    return NULL;
}

Value natives_call_method(VM *vm, Value receiver, ObjString *name, int argc, Value *args, bool *found){
    *found = false;
    if(!IS_OBJ(receiver))
        return NONE_VAL;

    method_table_t *table = NULL;
    switch(OBJ_TYPE(receiver)){
        case OBJ_LIST: table = &Tables[0]; break;
        case OBJ_MAP: table = &Tables[1]; break;
        case OBJ_ENUM: table = &Tables[2]; break;
        case OBJ_STRING: table = &Tables[3]; break;
        default: return NONE_VAL;
    }

    const method_t *method = method_find(table,name);
    if(method == NULL)
        return NONE_VAL;
    *found = true;
    return method->function(vm,receiver,argc,args);
}
