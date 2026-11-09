// Étape 1 du chapitre 8 : on alloue, on retient tout, on ne libère qu'à la fin.
#include <stdlib.h>

#include "include/gc.h"
#include "include/Value.h"
#include "include/scope.h"

static gc_object_t *all_objects;

void *gc_alloc(size_t size, gc_kind kind){
    gc_object_t *object = ceflo_alloc(size);
    object->kind = kind;
    object->next = all_objects;
    all_objects = object;
    return object;
}

void gc_free_all(void){
    while(all_objects != NULL){
        gc_object_t *next = all_objects->next;
        if(all_objects->kind == GC_SCOPE)
            freeScope((scope_t*)all_objects);
        else
            freeValue((value_t*)all_objects);
        free(all_objects);
        all_objects = next;
    }
}

// Ce dont le reste de l'interpréteur a besoin, et qui ne sert à rien tant qu'on ne libère pas.
void gc_add_bytes(size_t bytes){ (void)bytes; }
size_t gc_temp_mark(void){ return 0; }
void gc_temp_reset(size_t mark){ (void)mark; }
void gc_temp_push(void *object){ (void)object; }
void gc_root(void *object){ (void)object; }
