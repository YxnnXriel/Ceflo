#ifndef __LIST_H__
#define __LIST_H__

    #include "common.h"

    // Liste dynamique de pointeurs
    typedef struct LIST_STRUCT{
        void **Item;
        int Size;
        int Capacity;
    }list_t;

    list_t *newList(void);
    void List_Append(list_t *list, void *value);
    void *List_Get(list_t *list, int index);
    void List_Remove(list_t *list, int index);
    void List_Free(list_t *list);


    // Table de hachage : nom -> valeur.
    // Les clés ne sont pas copiées : elles doivent vivre aussi longtemps que la table.
    #define TAB_SIZE 8

    typedef struct HASH_STRUCT{
        const char *key;
        void *value;
        const char *type_name;  // type déclaré avec `::`, ou NULL
        bool is_const;
        uint32_t hash;
        struct HASH_STRUCT *next;
    }hash_t;

    typedef struct TABLE_STRUCT{
        hash_t *TabHash[TAB_SIZE];
    }table_t;

    uint32_t Hash_Name(const char *chaine);
    hash_t *Hash_Get(table_t *table, const char *key);
    hash_t *Hash_Get_Hashed(table_t *table, const char *key, uint32_t hash);
    hash_t *Hash_Set(table_t *table, const char *key, void *value);
    void Hash_Free(table_t *table);
    void Hash_Free_Pool(void);

#endif
