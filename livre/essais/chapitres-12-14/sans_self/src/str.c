#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/str.h"
#include "include/common.h"

String new_String(const char *str){
    return new_String_Lenght(str,(int)strlen(str));
}

String new_String_Lenght(const char *str, int lenght){
    // L'en-tête et le texte tiennent dans le même bloc.
    String string = ceflo_alloc(sizeof(*string) + (size_t)lenght + 1);
    string->str = (char*)(string + 1);
    memcpy(string->str,str,(size_t)lenght);
    string->str[lenght] = '\0';
    string->lenght = lenght;
    return string;
}

int String_free(String str){
    if(str == NULL)
        return 0;
    free(str);
    return 1;
}

char *readFile(const char *path){
    FILE *file = fopen(path,"rb");
    if(file == NULL)
        return NULL;

    buffer_t buffer;
    newBuffer(&buffer);

    char chunk[4096];
    size_t count;
    while((count = fread(chunk,1,sizeof(chunk),file)) > 0)
        Buffer_Append(&buffer,chunk,count);

    bool failed = ferror(file) != 0;
    fclose(file);

    if(failed){
        Buffer_Free(&buffer);
        return NULL;
    }

    return buffer.data;
}

void newBuffer(buffer_t *buffer){
    buffer->capacity = 64;
    buffer->size = 0;
    buffer->data = ceflo_alloc(buffer->capacity);
}

void Buffer_Append(buffer_t *buffer, const char *data, size_t size){
    if(buffer->size + size + 1 > buffer->capacity){
        while(buffer->size + size + 1 > buffer->capacity)
            buffer->capacity *= 2;
        buffer->data = ceflo_realloc(buffer->data,buffer->capacity);
    }
    memcpy(buffer->data + buffer->size,data,size);
    buffer->size += size;
    buffer->data[buffer->size] = '\0';
}

void Buffer_Append_Str(buffer_t *buffer, const char *str){
    Buffer_Append(buffer,str,strlen(str));
}

void Buffer_Append_Char(buffer_t *buffer, char carac){
    Buffer_Append(buffer,&carac,1);
}

void Buffer_Free(buffer_t *buffer){
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
    buffer->capacity = 0;
}
