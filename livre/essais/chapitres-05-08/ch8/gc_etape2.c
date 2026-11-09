// Étape 2 du chapitre 8 : mark and sweep, avec les racines seulement.
#include <stdlib.h>

#include "include/gc.h"
#include "include/Value.h"
#include "include/scope.h"

#define GC_FIRST_THRESHOLD (1024UL * 1024)

typedef struct{
    gc_object_t **items;
    size_t size;
    size_t capacity;
}gc_stack_t;

static gc_object_t *all_objects;
static size_t bytes_allocated;
static size_t next_collection = GC_FIRST_THRESHOLD;

static gc_stack_t roots;
static gc_stack_t gray;

static void gc_stack_push(gc_stack_t *stack, gc_object_t *object){
    if(stack->size == stack->capacity){
        stack->capacity = stack->capacity == 0 ? 256 : stack->capacity * 2;
        stack->items = ceflo_realloc(stack->items,stack->capacity * sizeof(gc_object_t*));
    }
    stack->items[stack->size] = object;
    stack->size += 1;
}

static void gc_mark(void *pointer){
    gc_object_t *object = pointer;
    if(object == NULL || object->marked)
        return;
    object->marked = true;
    gc_stack_push(&gray,object);
}

static void gc_trace_value(value_t *value){
    switch(value->type){
        case LIST:
            for(int i = 0; i < AS_LIST(value)->Size; i++)
                gc_mark(AS_LIST(value)->Item[i]);
            break;
        case MAP:
            for(int i = 0; i < AS_MAP(value)->size; i++){
                gc_mark(AS_MAP(value)->entries[i].key);
                gc_mark(AS_MAP(value)->entries[i].value);
            }
            break;
        case OBJECT:
            gc_mark(value->as.ObjectValue.self);
            gc_mark(value->as.ObjectValue.structure);
            break;
        case FUNCTION:
            gc_mark(value->as.functionValue.closure);
            break;
        case STRUCT:
            gc_mark(value->as.structValue.closure);
            break;
        case ENUM:
            gc_mark(value->as.enumValue.members);
            break;
        case ENUM_VALUE:
            gc_mark(value->as.enumItem.owner);
            break;
        default:
            break;
    }
}

static void gc_trace_scope(scope_t *scope){
    gc_mark(scope->scope_parent);
    for(int i = 0; i < TAB_SIZE; i++){
        for(hash_t *item = scope->variables.TabHash[i]; item != NULL; item = item->next)
            gc_mark(item->value);
    }
}

static void gc_free_object(gc_object_t *object){
    if(object->kind == GC_SCOPE)
        freeScope((scope_t*)object);
    else
        freeValue((value_t*)object);
    free(object);
}

static void gc_collect(void){
    for(size_t i = 0; i < roots.size; i++)
        gc_mark(roots.items[i]);
    while(gray.size > 0){
        gc_object_t *object = gray.items[--gray.size];
        if(object->kind == GC_VALUE)
            gc_trace_value((value_t*)object);
        else
            gc_trace_scope((scope_t*)object);
    }

    gc_object_t **link = &all_objects;
    while(*link != NULL){
        gc_object_t *object = *link;
        if(object->marked){
            object->marked = false;
            link = &object->next;
        }else{
            *link = object->next;
            gc_free_object(object);
        }
    }
    bytes_allocated = 0;
}

void *gc_alloc(size_t size, gc_kind kind){
    #ifdef CEFLO_GC_STRESS
        (void)next_collection;
        gc_collect();
    #else
        if(bytes_allocated > next_collection)
            gc_collect();
    #endif

    gc_object_t *object = ceflo_alloc(size);
    object->kind = kind;
    object->next = all_objects;
    all_objects = object;
    bytes_allocated += size;
    return object;
}

void gc_add_bytes(size_t bytes){ bytes_allocated += bytes; }
void gc_root(void *object){ gc_stack_push(&roots,object); }

// Pas encore de temporaires.
size_t gc_temp_mark(void){ return 0; }
void gc_temp_reset(size_t mark){ (void)mark; }
void gc_temp_push(void *object){ (void)object; }

void gc_free_all(void){
    while(all_objects != NULL){
        gc_object_t *next = all_objects->next;
        gc_free_object(all_objects);
        all_objects = next;
    }
    free(roots.items);
    free(gray.items);
}
