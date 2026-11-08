#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "common.h"

// La table n'est jamais remplie à plus de 3/4 : une recherche tombe vite sur une case vide.
#define TABLE_MAX_LOAD 0.75

/*
    Adressage ouvert, comme dans clox : une clé qui tombe sur une case prise
    essaie la suivante. Une clé effacée laisse une "pierre tombale" (key NULL,
    value true) pour que les recherches continuent à passer par-dessus.
*/

void table_init(Table *table){
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void table_free(Table *table){
    free(table->entries);
    table_init(table);
}

// La capacité est une puissance de 2 : `& (capacity - 1)` remplace le modulo.
static Entry *table_find_entry(Entry *entries, int capacity, ObjString *key){
    uint32_t mask = (uint32_t)capacity - 1;
    Entry *tombstone = NULL;

    for(uint32_t i = key->hash & mask; ; i = (i + 1) & mask){
        Entry *entry = &entries[i];
        if(entry->key == key)
            return entry;
        if(entry->key == NULL){
            if(IS_UNDEFINED(entry->value))
                return tombstone != NULL ? tombstone : entry;
            if(tombstone == NULL)
                tombstone = entry;
        }
    }
}

bool table_get(Table *table, ObjString *key, Value *value){
    if(table->count == 0)
        return false;

    Entry *entry = table_find_entry(table->entries,table->capacity,key);
    if(entry->key == NULL)
        return false;
    *value = entry->value;
    return true;
}

static void table_adjust_capacity(Table *table, int capacity){
    Entry *entries = ceflo_alloc(sizeof(Entry) * (size_t)capacity);
    for(int i = 0; i < capacity; i++)
        entries[i] = (Entry){NULL,UNDEFINED_VAL};

    // Les pierres tombales ne sont pas recopiées : count ne compte plus que les vraies clés.
    table->count = 0;
    for(int i = 0; i < table->capacity; i++){
        Entry *entry = &table->entries[i];
        if(entry->key == NULL)
            continue;
        Entry *dest = table_find_entry(entries,capacity,entry->key);
        *dest = *entry;
        table->count += 1;
    }

    free(table->entries);
    table->entries = entries;
    table->capacity = capacity;
}

bool table_set(Table *table, ObjString *key, Value value){
    if(table->count + 1 > table->capacity * TABLE_MAX_LOAD)
        table_adjust_capacity(table,table->capacity < 8 ? 8 : table->capacity * 2);

    Entry *entry = table_find_entry(table->entries,table->capacity,key);
    bool is_new = entry->key == NULL;
    // Une pierre tombale réutilisée était déjà comptée dans count.
    if(is_new && IS_UNDEFINED(entry->value))
        table->count += 1;

    entry->key = key;
    entry->value = value;
    return is_new;
}

bool table_delete(Table *table, ObjString *key){
    if(table->count == 0)
        return false;

    Entry *entry = table_find_entry(table->entries,table->capacity,key);
    if(entry->key == NULL)
        return false;

    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

// Le seul endroit où l'on compare des textes : c'est ce qui permet l'internage.
ObjString *table_find_string(Table *table, const char *chars, int length, uint32_t hash){
    if(table->count == 0)
        return NULL;

    uint32_t mask = (uint32_t)table->capacity - 1;
    for(uint32_t i = hash & mask; ; i = (i + 1) & mask){
        Entry *entry = &table->entries[i];
        if(entry->key == NULL){
            if(IS_UNDEFINED(entry->value))
                return NULL;
        }else if(entry->key->hash == hash && entry->key->length == length &&
            memcmp(entry->key->chars,chars,(size_t)length) == 0){
            return entry->key;
        }
    }
}
