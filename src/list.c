#include <stdlib.h>
#include <string.h>

#include "include/list.h"

#define TAB_OFFSET 2166136261u
#define TAB_PRIME 16777619u

list_t *newList(void){
    return ceflo_alloc(sizeof(list_t));
}

void List_Append(list_t *list, void *value){
    if(list->Size == list->Capacity){
        list->Capacity = list->Capacity == 0 ? 4 : list->Capacity * 2;
        list->Item = ceflo_realloc(list->Item,(size_t)list->Capacity * sizeof(void*));
    }
    list->Item[list->Size] = value;
    list->Size += 1;
}

void *List_Get(list_t *list, int index){
    if(index < 0 || index >= list->Size)
        return NULL;
    return list->Item[index];
}

void List_Remove(list_t *list, int index){
    if(index < 0 || index >= list->Size)
        return;
    memmove(
        &list->Item[index],
        &list->Item[index + 1],
        (size_t)(list->Size - index - 1) * sizeof(void*)
    );
    list->Size -= 1;
}

void List_Free(list_t *list){
    if(list == NULL)
        return;
    free(list->Item);
    free(list);
}

static pool_t hash_pool;

uint32_t Hash_Name(const char *chaine){
    uint32_t hash = TAB_OFFSET;
    for(; *chaine != '\0'; chaine++){
        hash ^= (unsigned char)*chaine;
        hash *= TAB_PRIME;
    }
    return hash;
}

hash_t *Hash_Get_Hashed(table_t *table, const char *key, uint32_t hash){
    for(hash_t *item = table->TabHash[hash % TAB_SIZE]; item != NULL; item = item->next){
        if(item->hash == hash && (item->key == key || strcmp(item->key,key) == 0))
            return item;
    }
    return NULL;
}

hash_t *Hash_Get(table_t *table, const char *key){
    return Hash_Get_Hashed(table,key,Hash_Name(key));
}

hash_t *Hash_Set(table_t *table, const char *key, void *value){
    uint32_t hash = Hash_Name(key);
    hash_t *item = Hash_Get_Hashed(table,key,hash);
    if(item == NULL){
        item = pool_alloc(&hash_pool,sizeof(*item));
        item->key = key;
        item->hash = hash;
        item->next = table->TabHash[hash % TAB_SIZE];
        table->TabHash[hash % TAB_SIZE] = item;
    }
    item->value = value;
    return item;
}

void Hash_Free(table_t *table){
    for(int i = 0; i < TAB_SIZE; i++){
        hash_t *item = table->TabHash[i];
        while(item != NULL){
            hash_t *next = item->next;
            pool_release(&hash_pool,item);
            item = next;
        }
        table->TabHash[i] = NULL;
    }
}

void Hash_Free_Pool(void){
    pool_free_all(&hash_pool);
}
