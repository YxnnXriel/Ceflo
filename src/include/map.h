#ifndef __MAP_H__
#define __MAP_H__

    /*
        Le type `map` de CeFlo : { nom: "Yann", age: 22 }

        C'est une table de hachage qui garde l'ordre d'insertion, comme le Map de
        TypeScript. Elle tient en deux tableaux :

            entries : les paires (clé, valeur), dans l'ordre où elles sont arrivées
            index   : pour un hash, la position de sa paire dans entries

        Une clé enlevée laisse un trou dans entries, jusqu'à la prochaine
        reconstruction. index est toujours au plus à moitié plein : une recherche
        s'arrête donc très vite sur une case vide.
    */

    #include "common.h"

    struct VALUE_STRUCT;

    typedef struct{
        struct VALUE_STRUCT *key;     // NULL : la paire a été enlevée
        struct VALUE_STRUCT *value;
        uint32_t hash;
    }map_entry_t;

    typedef struct{
        map_entry_t *entries;
        int32_t *index;
        int size;       // les cases utilisées dans entries, trous compris
        int count;      // les clés vivantes
        int capacity;
    }map_t;

    map_t *newMap(void);
    void freeMap(map_t *map);
    size_t map_bytes(map_t *map);

    // Les clés sont des chaînes ou des nombres. Ces fonctions retournent NULL si la clé n'existe pas.
    struct VALUE_STRUCT *map_get(map_t *map, struct VALUE_STRUCT *key);
    struct VALUE_STRUCT *map_get_text(map_t *map, const char *text, int lenght);
    struct VALUE_STRUCT *map_remove(map_t *map, struct VALUE_STRUCT *key);

    void map_set(map_t *map, struct VALUE_STRUCT *key, struct VALUE_STRUCT *value);
    // Remplace la valeur d'une clé qui existe déjà. Retourne false sinon.
    bool map_replace_text(map_t *map, const char *text, int lenght, struct VALUE_STRUCT *value);

#endif
