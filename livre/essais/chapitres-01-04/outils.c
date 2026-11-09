// Regarde grandir un buffer_t et une list_t.
#include <stdio.h>

#include "str.h"
#include "list.h"

int main(void){
    buffer_t buffer;
    newBuffer(&buffer);
    size_t capacity = buffer.capacity;
    printf("buffer : %zu octets réservés au départ\n",capacity);

    for(int i = 0; i < 1000; i++){
        Buffer_Append_Str(&buffer,"abc");
        if(buffer.capacity != capacity){
            capacity = buffer.capacity;
            printf("  après %4zu octets écrits : %zu réservés\n",buffer.size,capacity);
        }
    }
    Buffer_Free(&buffer);

    list_t *list = newList();
    int list_capacity = list->Capacity;
    printf("liste : %d cases au départ\n",list_capacity);
    for(int i = 0; i < 100; i++){
        List_Append(list,&buffer);
        if(list->Capacity != list_capacity){
            list_capacity = list->Capacity;
            printf("  au %3de élément : %d cases\n",list->Size,list_capacity);
        }
    }
    List_Free(list);
    return 0;
}
