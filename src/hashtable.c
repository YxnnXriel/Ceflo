#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "include/hashtable.h"


void newHt(ht_t *table){
    table->size = 0;

    for (int i = 0; i < HASH_LENGHT; i++)
        table->TabHash[i] = NULL;
    
}


static item_t *newItem(const char *key , ast_t *value){
    item_t *item = calloc(1,sizeof(*item));
    allowChar(&item->key,key);
    item->value = value;
    item->next = NULL;
    return item;
}

int Ht_append(ht_t *table , const char *key , ast_t *value){

    item_t *item = newItem(key,value);

    int index = Hash(item->key) % HASH_LENGHT;
    item->next = table->TabHash[index];
    table->TabHash[index] = item;

    table->size +=1;

    return 1;


}

ast_t *Ht_Get(ht_t *table,const char *key){
    
    int index = Hash(key) % HASH_LENGHT;
    item_t *l = table->TabHash[index];

    while (l != (void*)0 && strcmp(l->key,key) != 0){
        l = l->next;
    }

    return l != (void*)0 ? l->value:(void*)0;
}


void freeHt(ht_t *table){

}