#include <stdio.h>
#include <stdlib.h>
#include "include/debug.h"




    void Debug_ast_is_null(ast_t *node){
        printf("%s\n",node == NULL ? "OUI":"NON");
    }
    void Debug_Print(const char *func_name){
        printf("%s \n",func_name);
    }
    
    void Debug_Print_Ast_Type(ast_t *node){
        printf("%s \n",AST_TO_STRING[node->type]);
    }
