#include <string.h>

#include "memory.h"

#define MAP_FIRST_CAPACITY 8
#define MAP_EMPTY (-1)

/*
    Le map de CeFlo, porté de src/map.c. Une clé est une str, un number ou un
    élément d'énumération. Un char compte comme la str d'un seul octet :
    map_set range la str, et map_get cherche un char comme ce texte-là.

    Un élément d'énumération n'existe qu'une fois : il se compare par adresse.
    Une chaîne se compare par adresse, puis par contenu (un long texte n'est
    pas interné).
*/

ObjMap *map_new(VM *vm){
    ObjMap *map = (ObjMap*)vm_allocate_object(vm,sizeof(ObjMap),OBJ_MAP);
    map->entries = NULL;
    map->index = NULL;
    map->size = 0;
    map->count = 0;
    map->capacity = 0;
    return map;
}

bool map_key_is_valid(Value key){
    return IS_NUMBER(key) || IS_CHAR(key) || IS_STRING(key) || IS_ENUM_VALUE(key);
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

static uint32_t map_hash_bits(uint64_t bits){
    return map_mix((uint32_t)(bits ^ (bits >> 32)));
}

static uint32_t map_hash_key(Value key){
    switch(key.type){
        case VAL_NUMBER:{
            // `+ 0.0` : 0 et -0 sont le même nombre, donc la même clé.
            double number = AS_NUMBER(key) + 0.0;
            uint64_t bits;
            memcpy(&bits,&number,sizeof(bits));
            return map_hash_bits(bits);
        }
        case VAL_CHAR:
            return map_mix(string_hash(&AS_CHAR(key),1));
        default:
            if(IS_STRING(key))
                return map_mix(string_get_hash(AS_STRING(key)));
            return map_hash_bits((uint64_t)(uintptr_t)AS_OBJ(key));
    }
}

static bool map_same_key(Value stored, Value key){
    if(IS_CHAR(key))
        return IS_STRING(stored) && AS_STRING(stored)->length == 1 && AS_CSTRING(stored)[0] == AS_CHAR(key);
    if(stored.type != key.type)
        return false;
    if(IS_NUMBER(key))
        return AS_NUMBER(stored) == AS_NUMBER(key);
    if(IS_STRING(key))
        return IS_STRING(stored) && string_equals(AS_STRING(stored),AS_STRING(key));
    return AS_OBJ(stored) == AS_OBJ(key);
}


//                          ===================================== $RECHERCHE ==============================================


// La position de la paire dans entries, ou -1.
static int map_find(ObjMap *map, Value key, uint32_t hash){
    if(map->index == NULL)
        return -1;

    uint32_t mask = (uint32_t)map->capacity * 2 - 1;
    for(uint32_t i = hash & mask; map->index[i] != MAP_EMPTY; i = (i + 1) & mask){
        MapEntry *entry = &map->entries[map->index[i]];
        if(entry->hash == hash && map_same_key(entry->key,key))
            return map->index[i];
    }
    return -1;
}

bool map_get(ObjMap *map, Value key, Value *value){
    if(map->count == 0 || !map_key_is_valid(key))
        return false;

    int position = map_find(map,key,map_hash_key(key));
    if(position < 0)
        return false;
    *value = map->entries[position].value;
    return true;
}


//                          ===================================== $MODIFICATION ===========================================


static void map_index_add(ObjMap *map, uint32_t hash, int position){
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
static void map_rebuild(VM *vm, ObjMap *map){
    int capacity = MAP_FIRST_CAPACITY;
    while(capacity < map->count * 2)
        capacity *= 2;

    MapEntry *entries = VM_ALLOCATE(vm,MapEntry,capacity);
    int size = 0;
    for(int i = 0; i < map->size; i++){
        if(!IS_UNDEFINED(map->entries[i].key)){
            entries[size] = map->entries[i];
            size += 1;
        }
    }
    VM_FREE_ARRAY(vm,MapEntry,map->entries,map->capacity);
    VM_FREE_ARRAY(vm,int32_t,map->index,map->capacity * 2);

    map->entries = entries;
    map->size = size;
    map->capacity = capacity;
    map->index = VM_ALLOCATE(vm,int32_t,capacity * 2);
    memset(map->index,0xFF,sizeof(int32_t) * (size_t)capacity * 2);

    for(int i = 0; i < size; i++)
        map_index_add(map,entries[i].hash,i);
}

void map_set(VM *vm, ObjMap *map, Value key, Value value){
    if(!map_key_is_valid(key))
        return;

    uint32_t hash = map_hash_key(key);
    int position = map_find(map,key,hash);
    if(position >= 0){
        map->entries[position].value = value;
        return;
    }

    if(IS_CHAR(key))
        key = OBJ_VAL(string_copy(vm,&AS_CHAR(key),1));

    if(map->size == map->capacity)
        map_rebuild(vm,map);

    map->entries[map->size] = (MapEntry){.key = key, .value = value, .hash = hash};
    map_index_add(map,hash,map->size);
    map->size += 1;
    map->count += 1;
}

// La paire reste dans index : la recherche passe par-dessus jusqu'à la reconstruction.
bool map_remove(ObjMap *map, Value key){
    if(map->count == 0 || !map_key_is_valid(key))
        return false;

    int position = map_find(map,key,map_hash_key(key));
    if(position < 0)
        return false;

    map->entries[position].key = UNDEFINED_VAL;
    map->entries[position].value = NONE_VAL;
    map->count -= 1;
    return true;
}
