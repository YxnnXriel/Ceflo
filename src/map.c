#include <stdlib.h>
#include <string.h>

#include "include/map.h"
#include "include/Value.h"

#define MAP_FIRST_CAPACITY 8
#define MAP_EMPTY (-1)

map_t *newMap(void){
    return ceflo_alloc(sizeof(map_t));
}

void freeMap(map_t *map){
    free(map->entries);
    free(map->index);
    free(map);
}

size_t map_bytes(map_t *map){
    return sizeof(map_t) + (size_t)map->capacity * (sizeof(map_entry_t) + 2 * sizeof(int32_t));
}


//                          ===================================== $HASH ==================================================


// Mélange les bits : deux clés proches doivent tomber loin l'une de l'autre dans index.
static uint32_t map_mix(uint32_t hash){
    hash ^= hash >> 16;
    hash *= 0x85ebca6bu;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35u;
    return hash ^ (hash >> 16);
}

static uint32_t map_hash_text(const char *text, int lenght){
    uint32_t hash = 2166136261u;
    for(int i = 0; i < lenght; i++)
        hash = (hash ^ (unsigned char)text[i]) * 16777619u;

    hash = map_mix(hash);
    return hash != 0 ? hash : 1;
}

// `+ 0.0` : 0 et -0 sont le même nombre, donc la même clé.
static uint32_t map_hash_number(double number){
    number += 0.0;
    uint64_t bits;
    memcpy(&bits,&number,sizeof(bits));
    return map_mix((uint32_t)(bits ^ (bits >> 32)));
}

// Une chaîne ne change jamais : son hash se calcule une seule fois.
static uint32_t map_hash_key(value_t *key){
    if(key->type == NUMBER)
        return map_hash_number(AS_NUMBER(key));

    string_t *string = AS_STRING(key);
    if(string->hash == 0)
        string->hash = map_hash_text(string->str,string->lenght);
    return string->hash;
}


//                          ===================================== $RECHERCHE ==============================================


static bool map_is_text(value_t *key, const char *text, int lenght){
    return key->type == STRING && AS_STRING(key)->lenght == lenght &&
        memcmp(AS_STRING(key)->str,text,(size_t)lenght) == 0;
}

static int map_find_text(map_t *map, uint32_t hash, const char *text, int lenght){
    if(map->index == NULL)
        return -1;

    uint32_t mask = (uint32_t)map->capacity * 2 - 1;
    for(uint32_t i = hash & mask; map->index[i] != MAP_EMPTY; i = (i + 1) & mask){
        map_entry_t *entry = &map->entries[map->index[i]];
        if(entry->hash == hash && entry->key != NULL && map_is_text(entry->key,text,lenght))
            return map->index[i];
    }
    return -1;
}

static int map_find_number(map_t *map, uint32_t hash, double number){
    if(map->index == NULL)
        return -1;

    uint32_t mask = (uint32_t)map->capacity * 2 - 1;
    for(uint32_t i = hash & mask; map->index[i] != MAP_EMPTY; i = (i + 1) & mask){
        map_entry_t *entry = &map->entries[map->index[i]];
        if(entry->hash == hash && entry->key != NULL && entry->key->type == NUMBER && AS_NUMBER(entry->key) == number)
            return map->index[i];
    }
    return -1;
}

static int map_find(map_t *map, value_t *key, uint32_t hash){
    if(key->type == NUMBER)
        return map_find_number(map,hash,AS_NUMBER(key));
    return map_find_text(map,hash,AS_STRING(key)->str,AS_STRING(key)->lenght);
}

value_t *map_get(map_t *map, value_t *key){
    int position = map_find(map,key,map_hash_key(key));
    return position >= 0 ? map->entries[position].value : NULL;
}

value_t *map_get_text(map_t *map, const char *text, int lenght){
    int position = map_find_text(map,map_hash_text(text,lenght),text,lenght);
    return position >= 0 ? map->entries[position].value : NULL;
}


//                          ===================================== $MODIFICATION ===========================================


static void map_index_add(map_t *map, uint32_t hash, int position){
    uint32_t mask = (uint32_t)map->capacity * 2 - 1;
    uint32_t i = hash & mask;
    while(map->index[i] != MAP_EMPTY)
        i = (i + 1) & mask;
    map->index[i] = position;
}

/**
 * @brief refait les deux tableaux quand entries est plein : les trous disparaissent,
 *        et la capacité suit le nombre de clés vivantes.
 */
static void map_rebuild(map_t *map){
    size_t before = map_bytes(map);

    int capacity = MAP_FIRST_CAPACITY;
    while(capacity < map->count * 2)
        capacity *= 2;

    map_entry_t *entries = ceflo_alloc((size_t)capacity * sizeof(map_entry_t));
    int size = 0;
    for(int i = 0; i < map->size; i++){
        if(map->entries[i].key != NULL){
            entries[size] = map->entries[i];
            size += 1;
        }
    }
    free(map->entries);
    free(map->index);

    map->entries = entries;
    map->size = size;
    map->capacity = capacity;
    map->index = ceflo_alloc((size_t)capacity * 2 * sizeof(int32_t));
    memset(map->index,0xFF,(size_t)capacity * 2 * sizeof(int32_t));

    for(int i = 0; i < size; i++)
        map_index_add(map,entries[i].hash,i);

    if(map_bytes(map) > before)
        gc_add_bytes(map_bytes(map) - before);
}

void map_set(map_t *map, value_t *key, value_t *value){
    uint32_t hash = map_hash_key(key);
    int position = map_find(map,key,hash);

    if(position >= 0){
        map->entries[position].value = value;
        return;
    }

    if(map->size == map->capacity)
        map_rebuild(map);

    map->entries[map->size] = (map_entry_t){.key = key, .value = value, .hash = hash};
    map_index_add(map,hash,map->size);
    map->size += 1;
    map->count += 1;
}

bool map_replace_text(map_t *map, const char *text, int lenght, value_t *value){
    int position = map_find_text(map,map_hash_text(text,lenght),text,lenght);
    if(position < 0)
        return false;
    map->entries[position].value = value;
    return true;
}

value_t *map_remove(map_t *map, value_t *key){
    int position = map_find(map,key,map_hash_key(key));
    if(position < 0)
        return NULL;

    value_t *value = map->entries[position].value;
    map->entries[position].key = NULL;
    map->entries[position].value = NULL;
    map->count -= 1;
    return value;
}
