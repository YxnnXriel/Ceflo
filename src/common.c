#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/common.h"



void allowChar(char ** chaine , const char *value){
    *chaine = calloc(strlen(value), sizeof(char*));
    strcpy(*chaine,value);
}
