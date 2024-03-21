#include <stdio.h>
#include <stdlib.h>
#include "include/debug.h"




    void Debug_ast_is_null(ast_t *node){
        printf("%s\n",node == NULL ? "OUI":"NON");
    }
    void Debug_Print(const char *func_name){
        printf("%s \n",func_name);
    }
    
    uint8_t Debug_Print_Ast_Type(ast_t *node){
        printf("DEBUG : %s \n",AST_TO_STRING[node->type]);
        return node->type;
    }

    void Debug_value_is_null(void *x){
        printf("%s\n",x == NULL ? "OUI":"NON");
    }
