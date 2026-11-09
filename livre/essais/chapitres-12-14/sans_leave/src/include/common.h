#ifndef __COMMON_H__
#define __COMMON_H__

    #include <stdbool.h>
    #include <stdint.h>
    #include <stddef.h>

    // Allocations : le programme s'arrête proprement si la mémoire manque.
    void *ceflo_alloc(size_t size);
    void *ceflo_realloc(void *pointer, size_t size);
    void allowChar(char **chaine, const char *value);

    /*
        Un réservoir d'objets de même taille. Un script crée et jette des millions
        de petites valeurs : les prendre ici est bien plus rapide que malloc. Un
        objet rendu resservira au prochain pool_alloc.

        Avec CEFLO_NO_POOL, chaque objet passe par malloc : c'est ce que font
        `make sanitize` et `make val`, pour voir un objet utilisé après sa libération.
    */
    typedef struct{
        void *free_items;
        struct POOL_BLOCK *blocks;
        size_t left;       // les objets encore jamais servis dans le dernier bloc
    }pool_t;

    void *pool_alloc(pool_t *pool, size_t size);
    void pool_release(pool_t *pool, void *item);
    void pool_free_all(pool_t *pool);

    // Garde-fou de la pile C : un interpréteur récursif ne doit jamais segfault.
    void ceflo_stack_init(void);
    bool ceflo_stack_exceeded(void);

#endif
