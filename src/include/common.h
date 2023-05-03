#ifndef __COMMON_H__
#define __COMMON_H__


    #include <stdbool.h>
    #include <stdint.h>
    #include "str.h"

    void allowChar(char ** chaine , const char *value);

    void GetString(char **chaine ,char *source  , int start, int end);

    string_t * GetStringForStr(char *source  , int start, int end);

#endif