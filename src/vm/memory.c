#include <stdlib.h>

#include "memory.h"
#include "common.h"

// Le seuil minimal : en dessous, collecter souvent coûte plus que la mémoire gagnée.
#define GC_FIRST_THRESHOLD (16UL * 1024 * 1024)
#define GC_GROW_FACTOR 2

/*
    Les petits objets (64 octets au plus) viennent de réservoirs, un par taille
    arrondie à 16 octets : un script en crée et en jette des millions, et
    pool_alloc est bien plus rapide que malloc. Avec CEFLO_NO_POOL, pool_alloc
    passe par malloc et ASan voit chaque objet.
*/
#define POOL_STEP 16
#define POOL_COUNT 4

static pool_t pools[POOL_COUNT];

static pool_t *memory_pool(size_t size){
    size_t index = (size + POOL_STEP - 1) / POOL_STEP - 1;
    return index < POOL_COUNT ? &pools[index] : NULL;
}

static size_t memory_pool_size(size_t size){
    return (size + POOL_STEP - 1) / POOL_STEP * POOL_STEP;
}


//                          ===================================== $ALLOCATION ==============================================


void *vm_reallocate(VM *vm, void *pointer, size_t old_size, size_t new_size){
    // Jamais de collecte ici : seulement aux points sûrs.
    vm->bytes_allocated += new_size;
    vm->bytes_allocated = vm->bytes_allocated > old_size ? vm->bytes_allocated - old_size : 0;

    if(new_size == 0){
        free(pointer);
        return NULL;
    }
    return ceflo_realloc(pointer,new_size);
}

Obj *vm_allocate_object(VM *vm, size_t size, ObjType type){
    pool_t *pool = memory_pool(size);
    Obj *object = pool != NULL ? pool_alloc(pool,memory_pool_size(size)) : ceflo_realloc(NULL,size);
    vm->bytes_allocated += size;

    object->type = type;
    object->marked = false;
    object->printing = false;
    object->next = vm->objects;
    vm->objects = object;
    return object;
}

// La taille passée à vm_allocate_object.
static size_t object_size(Obj *object){
    switch(object->type){
        case OBJ_STRING: return sizeof(ObjString) + (size_t)((ObjString*)object)->length + 1;
        case OBJ_LIST: return sizeof(ObjList);
        case OBJ_MAP: return sizeof(ObjMap);
        case OBJ_FUNCTION: return sizeof(ObjFunction);
        case OBJ_CLOSURE: return sizeof(ObjClosure);
        case OBJ_UPVALUE: return sizeof(ObjUpvalue);
        case OBJ_NATIVE: return sizeof(ObjNative);
        case OBJ_BOUND_METHOD: return sizeof(ObjBoundMethod);
        case OBJ_STRUCT: return sizeof(ObjStruct);
        case OBJ_INSTANCE:
            return sizeof(ObjInstance) + sizeof(Value) * (size_t)((ObjInstance*)object)->structure->field_count;
        case OBJ_ENUM: return sizeof(ObjEnum);
        case OBJ_ENUM_VALUE: return sizeof(ObjEnumValue);
        case OBJ_MODULE: return sizeof(ObjModule);
        case OBJ_GENERIC: return sizeof(ObjGeneric);
    }
    return 0;
}

// Libère ce que l'objet possède, puis l'objet lui-même.
static void object_free(VM *vm, Obj *object){
    switch(object->type){
        case OBJ_LIST:{
            ObjList *list = (ObjList*)object;
            VM_FREE_ARRAY(vm,Value,list->items,list->capacity);
            break;
        }
        case OBJ_MAP:{
            ObjMap *map = (ObjMap*)object;
            VM_FREE_ARRAY(vm,MapEntry,map->entries,map->capacity);
            VM_FREE_ARRAY(vm,int32_t,map->index,map->capacity * 2);
            break;
        }
        case OBJ_FUNCTION:{
            ObjFunction *function = (ObjFunction*)object;
            chunk_free(&function->chunk);
            free(function->param_types);    // alloué par le compilateur
            break;
        }
        case OBJ_CLOSURE:{
            ObjClosure *closure = (ObjClosure*)object;
            VM_FREE_ARRAY(vm,ObjUpvalue*,closure->upvalues,closure->upvalue_count);
            break;
        }
        case OBJ_STRUCT:{
            ObjStruct *structure = (ObjStruct*)object;
            VM_FREE_ARRAY(vm,ObjString*,structure->field_names,structure->field_count);
            VM_FREE_ARRAY(vm,TypeRef,structure->field_types,structure->field_count);
            VM_FREE_ARRAY(vm,bool,structure->field_is_const,structure->field_count);
            table_free(&structure->methods);
            break;
        }
        case OBJ_ENUM:{
            ObjEnum *enumeration = (ObjEnum*)object;
            VM_FREE_ARRAY(vm,ObjEnumValue*,enumeration->items,enumeration->count);
            break;
        }
        case OBJ_MODULE:{
            ObjModule *module = (ObjModule*)object;
            value_array_free(&module->globals);
            VM_FREE_ARRAY(vm,ObjString*,module->names,module->capacity);
            VM_FREE_ARRAY(vm,TypeRef,module->types,module->capacity);
            VM_FREE_ARRAY(vm,bool,module->is_const,module->capacity);
            table_free(&module->indexes);
            free(module->exports);          // alloué par la VM (OP_EXPORT)
            break;
        }
        default:
            break;
    }

    // La taille se lit avant de libérer quoi que ce soit d'autre : une instance a besoin de sa structure.
    size_t size = object_size(object);
    vm->bytes_allocated = vm->bytes_allocated > size ? vm->bytes_allocated - size : 0;
    pool_t *pool = memory_pool(size);
    if(pool != NULL)
        pool_release(pool,object);
    else
        free(object);
}


//                          ===================================== $MARQUAGE ==============================================


void gc_mark_object(VM *vm, Obj *object){
    if(object == NULL || object->marked)
        return;
    object->marked = true;

    // Une chaîne, une native, un generic ne contiennent rien d'autre.
    if(object->type == OBJ_STRING || object->type == OBJ_NATIVE || object->type == OBJ_GENERIC)
        return;

    if(vm->gray_count == vm->gray_capacity){
        vm->gray_capacity = GROW_CAPACITY(vm->gray_capacity);
        vm->gray_stack = ceflo_realloc(vm->gray_stack,sizeof(Obj*) * (size_t)vm->gray_capacity);
    }
    vm->gray_stack[vm->gray_count] = object;
    vm->gray_count += 1;
}

void gc_mark_value(VM *vm, Value value){
    if(IS_OBJ(value))
        gc_mark_object(vm,AS_OBJ(value));
}

void gc_mark_table(VM *vm, Table *table){
    for(int i = 0; i < table->capacity; i++){
        Entry *entry = &table->entries[i];
        if(entry->key == NULL)
            continue;
        gc_mark_object(vm,(Obj*)entry->key);
        gc_mark_value(vm,entry->value);
    }
}

static void gc_mark_array(VM *vm, Value *values, int count){
    for(int i = 0; i < count; i++)
        gc_mark_value(vm,values[i]);
}

static void gc_mark_type(VM *vm, TypeRef *type){
    gc_mark_object(vm,(Obj*)type->name);
}

static void gc_mark_chunk(VM *vm, Chunk *chunk){
    gc_mark_array(vm,chunk->constants.values,chunk->constants.count);
    for(int i = 0; i < chunk->check_count; i++){
        gc_mark_type(vm,&chunk->checks[i].type);
        gc_mark_object(vm,(Obj*)chunk->checks[i].subject);
    }
}

// Marque tout ce que contient un objet déjà marqué.
static void gc_blacken(VM *vm, Obj *object){
    switch(object->type){
        case OBJ_LIST:{
            ObjList *list = (ObjList*)object;
            gc_mark_array(vm,list->items,list->count);
            break;
        }
        case OBJ_MAP:{
            ObjMap *map = (ObjMap*)object;
            for(int i = 0; i < map->size; i++){
                gc_mark_value(vm,map->entries[i].key);
                gc_mark_value(vm,map->entries[i].value);
            }
            break;
        }
        case OBJ_FUNCTION:{
            ObjFunction *function = (ObjFunction*)object;
            gc_mark_object(vm,(Obj*)function->name);
            gc_mark_object(vm,(Obj*)function->module);
            gc_mark_chunk(vm,&function->chunk);
            if(function->param_types != NULL){
                for(int i = 0; i < function->arity; i++)
                    gc_mark_type(vm,&function->param_types[i]);
            }
            gc_mark_type(vm,&function->return_type);
            break;
        }
        case OBJ_CLOSURE:{
            ObjClosure *closure = (ObjClosure*)object;
            gc_mark_object(vm,(Obj*)closure->function);
            for(int i = 0; i < closure->upvalue_count; i++)
                gc_mark_object(vm,(Obj*)closure->upvalues[i]);
            break;
        }
        case OBJ_UPVALUE:
            gc_mark_value(vm,((ObjUpvalue*)object)->closed);
            break;
        case OBJ_BOUND_METHOD:{
            ObjBoundMethod *bound = (ObjBoundMethod*)object;
            gc_mark_value(vm,bound->receiver);
            gc_mark_value(vm,bound->method);
            break;
        }
        case OBJ_STRUCT:{
            ObjStruct *structure = (ObjStruct*)object;
            gc_mark_object(vm,(Obj*)structure->name);
            for(int i = 0; i < structure->field_count; i++){
                gc_mark_object(vm,(Obj*)structure->field_names[i]);
                gc_mark_type(vm,&structure->field_types[i]);
            }
            gc_mark_value(vm,structure->fields_init);
            gc_mark_table(vm,&structure->methods);
            gc_mark_array(vm,structure->inits,8);
            gc_mark_value(vm,structure->str_method);
            gc_mark_value(vm,structure->cast_method);
            break;
        }
        case OBJ_INSTANCE:{
            ObjInstance *instance = (ObjInstance*)object;
            gc_mark_object(vm,(Obj*)instance->structure);
            gc_mark_array(vm,instance->fields,instance->structure->field_count);
            break;
        }
        case OBJ_ENUM:{
            ObjEnum *enumeration = (ObjEnum*)object;
            gc_mark_object(vm,(Obj*)enumeration->name);
            for(int i = 0; i < enumeration->count; i++)
                gc_mark_object(vm,(Obj*)enumeration->items[i]);
            break;
        }
        case OBJ_ENUM_VALUE:{
            ObjEnumValue *item = (ObjEnumValue*)object;
            gc_mark_value(vm,item->owner);
            gc_mark_object(vm,(Obj*)item->name);
            gc_mark_value(vm,item->value);
            break;
        }
        case OBJ_MODULE:{
            ObjModule *module = (ObjModule*)object;
            gc_mark_object(vm,(Obj*)module->path);
            gc_mark_array(vm,module->globals.values,module->globals.count);
            for(int i = 0; i < module->globals.count; i++){
                gc_mark_object(vm,(Obj*)module->names[i]);
                gc_mark_type(vm,&module->types[i]);
            }
            gc_mark_table(vm,&module->indexes);
            for(int i = 0; i < module->export_count; i++)
                gc_mark_object(vm,(Obj*)module->exports[i]);
            break;
        }
        case OBJ_STRING:
        case OBJ_NATIVE:
        case OBJ_GENERIC:
            break;
    }
}

static void gc_mark_roots(VM *vm){
    for(Value *slot = vm->stack; slot < vm->stack_top; slot++)
        gc_mark_value(vm,*slot);
    for(int i = 0; i < vm->frame_count; i++)
        gc_mark_object(vm,(Obj*)vm->frames[i].closure);
    for(ObjUpvalue *upvalue = vm->open_upvalues; upvalue != NULL; upvalue = upvalue->next)
        gc_mark_object(vm,(Obj*)upvalue);
    for(int i = 0; i < vm->root_count; i++)
        gc_mark_value(vm,*vm->roots[i]);

    gc_mark_table(vm,&vm->builtins);
    gc_mark_table(vm,&vm->modules);
    gc_mark_table(vm,&vm->native_modules);

    gc_mark_object(vm,(Obj*)vm->name_init);
    gc_mark_object(vm,(Obj*)vm->name_str);
    gc_mark_object(vm,(Obj*)vm->name_cast);
    gc_mark_object(vm,(Obj*)vm->name_main);
}


//                          ===================================== $COLLECTE ==============================================


// Une chaîne internée que plus rien n'utilise quitte la table avant d'être libérée.
static void gc_remove_white_strings(Table *table){
    for(int i = 0; i < table->capacity; i++){
        Entry *entry = &table->entries[i];
        // Une pierre tombale, comme table_delete, sans refaire la recherche.
        if(entry->key != NULL && !entry->key->obj.marked){
            entry->key = NULL;
            entry->value = BOOL_VAL(true);
        }
    }
}

static void gc_sweep(VM *vm){
    Obj **link = &vm->objects;
    while(*link != NULL){
        Obj *object = *link;
        if(object->marked){
            object->marked = false;
            link = &object->next;
        }else{
            *link = object->next;
            object_free(vm,object);
        }
    }
}

void gc_collect(VM *vm){
    gc_mark_roots(vm);
    while(vm->gray_count > 0){
        vm->gray_count -= 1;
        gc_blacken(vm,vm->gray_stack[vm->gray_count]);
    }
    gc_remove_white_strings(&vm->strings);
    gc_sweep(vm);

    vm->next_gc = vm->bytes_allocated * GC_GROW_FACTOR;
    if(vm->next_gc < GC_FIRST_THRESHOLD)
        vm->next_gc = GC_FIRST_THRESHOLD;
}

void gc_safepoint(VM *vm){
    #ifdef CEFLO_GC_STRESS
        gc_collect(vm);
    #else
        if(vm->bytes_allocated <= vm->next_gc)
            return;
        // vm_init n'a pas choisi de seuil : on prend celui de départ.
        if(vm->next_gc == 0){
            vm->next_gc = GC_FIRST_THRESHOLD;
            if(vm->bytes_allocated <= vm->next_gc)
                return;
        }
        gc_collect(vm);
    #endif
}

void gc_free_objects(VM *vm){
    Obj *object = vm->objects;
    while(object != NULL){
        Obj *next = object->next;
        object_free(vm,object);
        object = next;
    }
    vm->objects = NULL;

    free(vm->gray_stack);
    vm->gray_stack = NULL;
    vm->gray_count = 0;
    vm->gray_capacity = 0;

    for(int i = 0; i < POOL_COUNT; i++)
        pool_free_all(&pools[i]);
}
