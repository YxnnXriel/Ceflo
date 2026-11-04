#ifndef __GC_H__
#define __GC_H__

    /*
        Le garbage collector de CeFlo : un simple "mark and sweep".

        Tout ce qui est alloué pendant l'exécution (valeurs et portées) passe par
        gc_alloc. Quand trop de mémoire est utilisée, on marque ce qui est encore
        atteignable, puis on libère tout le reste.

        Pendant qu'une instruction s'exécute, ses valeurs intermédiaires ne sont
        rangées nulle part : elles vivent sur la pile C. Elles sont donc aussi
        retenues dans la "pile des temporaires". Le visitor vide cette pile à la
        fin de chaque instruction : ainsi, le reste du code ne se soucie jamais de
        la mémoire.
    */

    #include "common.h"

    typedef enum{
        GC_VALUE,
        GC_SCOPE,
    }gc_kind;

    typedef struct GC_OBJECT{
        struct GC_OBJECT *next;
        gc_kind kind;
        bool marked;
        bool printing;   // évite de boucler en affichant une liste qui se contient
    }gc_object_t;

    void *gc_alloc(size_t size, gc_kind kind);
    void gc_add_bytes(size_t bytes);

    size_t gc_temp_mark(void);
    void gc_temp_reset(size_t mark);
    void gc_temp_push(void *object);

    // Garde un objet en vie jusqu'à la fin du programme.
    void gc_root(void *object);

    void gc_free_all(void);

#endif
