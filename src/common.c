#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/common.h"



void allowChar(char ** chaine , const char *value){
    *chaine = calloc(strlen(value), sizeof(char*));
    strcpy(*chaine,value);
}


void GetString(char **chaine ,char *source  , int start, int end){

    *chaine = calloc(start + end + 1,sizeof(char));
    memcpy(
        *chaine,
        &source[start],
        end
    );
}


string_t * GetStringForStr(char *source  , int start, int end){
    char *chaine = NULL;
    GetString(&chaine,source,start,end);
    String res = new_String(chaine);
    free(chaine);
    return res;
}
