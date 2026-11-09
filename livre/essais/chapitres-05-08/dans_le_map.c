#include <stdio.h>
#include "Value.h"

static void montre(map_t *map){
    printf("size=%d count=%d capacity=%d\n", map->size, map->count, map->capacity);
    printf("  entries :");
    for(int i = 0; i < map->size; i++){
        value_t *key = map->entries[i].key;
        printf(" [%d] %s", i, key != NULL ? AS_STRING(key)->str : "(trou)");
    }
    printf("\n  index   :");
    for(int i = 0; i < map->capacity * 2; i++){
        if(map->index[i] == -1) printf(" .");
        else printf(" %d", map->index[i]);
    }
    printf("\n");
}

int main(void){
    value_t *t = makeMap();
    Value_map_set_name(t, "nom", makeString("Yann"));
    Value_map_set_name(t, "age", makeNumber(22));
    Value_map_set_name(t, "ville", makeString("Abidjan"));
    montre(AS_MAP(t));

    map_remove(AS_MAP(t), makeString("age"));
    montre(AS_MAP(t));

    Value_map_set_name(t, "pays", makeString("CI"));
    montre(AS_MAP(t));

    const char *noms[] = {"a", "b", "c", "d", "e"};
    for(int i = 0; i < 5; i++)
        Value_map_set_name(t, noms[i], makeNumber(i));
    printf("après a..e : size=%d count=%d capacity=%d\n", AS_MAP(t)->size, AS_MAP(t)->count, AS_MAP(t)->capacity);

    Value_map_set_name(t, "f", makeNumber(5));
    montre(AS_MAP(t));
    return 0;
}
