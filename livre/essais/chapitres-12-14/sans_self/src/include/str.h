#ifndef __STR_H__
#define __STR_H__

    #include <stddef.h>
    #include <stdint.h>

    typedef struct STRUCT_STR{
        char *str;
        int lenght;
        uint32_t hash;   // calculé au premier besoin par map.c, 0 tant qu'il ne l'est pas
    }string_t, *String;

    String new_String(const char *str);
    String new_String_Lenght(const char *str, int lenght);
    int String_free(String str);

    // Lit un fichier entier. Retourne NULL s'il ne peut pas être lu.
    char *readFile(const char *path);


    // Tampon qui grandit tout seul, pour construire les chaînes.
    typedef struct STRUCT_BUFFER{
        char *data;
        size_t size;
        size_t capacity;
    }buffer_t;

    void newBuffer(buffer_t *buffer);
    void Buffer_Append(buffer_t *buffer, const char *data, size_t size);
    void Buffer_Append_Str(buffer_t *buffer, const char *str);
    void Buffer_Append_Char(buffer_t *buffer, char carac);
    void Buffer_Free(buffer_t *buffer);

#endif
