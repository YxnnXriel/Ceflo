#ifndef __SCOPE_H__
#define __SCOPE_H__

    #include "gc.h"
    #include "list.h"

    struct VALUE_STRUCT;

    // Une portée : les noms visibles à un endroit du programme, et un lien vers
    // la portée qui l'entoure. Les fonctions, les structures et les variables
    // partagent les mêmes noms.
    typedef struct SCOPE_STRUCT{
        gc_object_t gc;
        table_t variables;
        struct SCOPE_STRUCT *scope_parent;
        list_t *exports;     // noms publiés par `export`, si c'est un fichier
    }scope_t;

    scope_t *newScope(scope_t *parent);
    void freeScope(scope_t *scope);

    // Cherche seulement dans cette portée
    hash_t *scope_get(scope_t *scope, const char *name);
    // Cherche dans cette portée, puis dans celles qui l'entourent
    hash_t *scope_lookup(scope_t *scope, const char *name);
    hash_t *scope_lookup_hashed(scope_t *scope, const char *name, uint32_t hash);
    hash_t *scope_define(scope_t *scope, const char *name, struct VALUE_STRUCT *value);

#endif
