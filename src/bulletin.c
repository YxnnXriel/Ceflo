#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/bulletin.h"





ast_t * bulletin_println(visitor_t * visitor , ast_t * node , list_ast_t *args ){

    int i = 0;
    for ( i = 0; i < (int) args->size; i++){
        ast_t * child = List_Ast_Get(args,i);
        child = visitor_visit(visitor,child);
        printValueAst(child);
    }
    
    printf("\n");

    ast_t * count = newAst(AST_NUMBER);
    count->value.as.numberValue = i;
    count->value.type = NUMBER;

    return count; 
}

ast_t * bulletin_print(visitor_t * visitor , ast_t * node , list_ast_t *args ){

    int i = 0;
    for ( i = 0; i < (int) args->size; i++){
        ast_t * child = List_Ast_Get(args,i);
        child = visitor_visit(visitor,child);
        printValueAst(child);
    }
    
    ast_t * count = newAst(AST_NUMBER);
    count->value.as.numberValue = i;
    count->value.type = NUMBER;

    return count; 
}


ast_t * bulletin_input(visitor_t * visitor , ast_t * node , list_ast_t *args ){
    bulletin_print(visitor, node, args);

    char buffer[BUFSIZ];
    fgets(buffer, BUFSIZ, stdin);

    // Supprimer le caractère de nouvelle ligne de la chaîne saisie
    buffer[strlen(buffer) - 1] = '\0';

    ast_t *string = newAst(AST_STRING);
    string->value.as.stringValue = new_String(buffer);

    return string;
}


ast_t * bulletin_printaddr(visitor_t * visitor , ast_t * node , list_ast_t *args ){

    for (int i = 0; i < (int) args->size; i++){
        ast_t * child = List_Ast_Get(args,i);
        child = visitor_visit(visitor,child);
        printf("%p",(void*)child);
    }

    printf("\n");

    return node;

}

ast_t *bulletin_switch_function(visitor_t * visitor , ast_t *node , list_ast_t *args){
    if(node->function_call_name == NULL)
        return NULL;
    const char * function_name = node->function_call_name;

    for (int i = 0; i < BULLETIN_LIST_COUNT -1; i++)
        if(strcmp(function_name,Bulletin_List[i].name) == 0)
            return Bulletin_List[i].func(visitor,node,&node->function_call_args);

        
    return NULL;
}
