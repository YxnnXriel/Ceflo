#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

    #include "ast.h"
    #include "common.h"
    #include "list.h"

    #define HASH_LENGHT 256

    typedef struct ITEM_STRUCT{
        char *key;
        ast_t *value;
        struct ITEM_STRUCT *next;
    }item_t;
    

    typedef struct HASHTABLE_STRUCT{
        int size;
        item_t *TabHash[HASH_LENGHT];
    }ht_t;
    
    void newHt(ht_t *table);
    int Ht_append(ht_t *table , const char *key , ast_t *value);
    ast_t *Ht_Get(ht_t *table,const char *key);
    void freeHt(ht_t *table);


#endif