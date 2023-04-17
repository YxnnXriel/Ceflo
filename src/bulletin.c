#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/bulletin.h"





ast_t * bulletin_println(visitor_t * visitor , ast_t * node , list_ast_t *args ){

    for (int i = 0; i < (int) args->size; i++){
        ast_t * child = List_Ast_Get(args,i);
        child = visitor_visit(visitor,child);
        printValueAst(child,visitor);
    }
    
    printf("\n");

    return node; 
}


ast_t *bulletin_switch_function(visitor_t * visitor , ast_t *node , list_ast_t *args){

    const char * function_name = node->function_call_name;

    for (int i = 0; i < BULLETIN_LIST_COUNT -1; i++)
        if(strcmp(function_name,Bulletin_List[i].name) == 0)
            return Bulletin_List[i].func(visitor,node,&node->function_call_args);

        
    return NULL;
}
