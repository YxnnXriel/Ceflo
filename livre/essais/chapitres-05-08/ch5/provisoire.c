// En attendant le chapitre 7 (le visitor) et le chapitre 8 (le GC) : juste de quoi compiler.
#include <stdlib.h>
#include "Value.h"
#include "error.h"

void *gc_alloc(size_t size, gc_kind kind){
    gc_object_t *object = ceflo_alloc(size);
    object->kind = kind;
    return object;
}
void gc_add_bytes(size_t bytes){ (void)bytes; }
void gc_root(void *object){ (void)object; }
void gc_temp_push(void *object){ (void)object; }

// Value.c appelle __str__ d'un objet avec visitor_call : pas d'objets avant le chapitre 7.
struct VALUE_STRUCT *visitor_call(struct VISITOR_STRUCT *visitor, ast_t *node, struct VALUE_STRUCT *function, struct VALUE_STRUCT **args, int argc){
    (void)visitor;(void)function;(void)args;(void)argc;
    visitor_error(node,"pas encore de fonctions CeFlo");
}
