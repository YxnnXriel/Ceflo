#include "../../src/include/list.h"
#include "../../src/include/ast.h"
#include "../../src/include/visitor.h"
#include "../../src/include/str.h"


/*
    1 -> @str
    2 -> @list str
*/


ast_t *TextTable(visitor_t *visitor , ast_t *self , list_t *args){

    if(args->Size < 2){
        printf("TextTable(<str:: Champ> , ....)\n");
        exit(1);
    }

    ast_t *list_champ = visitor_visit(visitor,(ast_t*)List_Get(args,0));
    ast_t *args_str = (void*)0;

    if(list_champ->type != AST_STRING ){
        printf("TextTable(<str:: Champ> , ....)\n");
        exit(1);
    }

    listString_t *list = new_ListString();

    for (int i = 1; i < args->Size; i++){

        args_str = visitor_visit(visitor,(ast_t*)List_Get(args,i));
        if(args_str->type != AST_STRING ){
            printf("TextTable -> args:%d == %s \n",i,AST_TO_STRING[args_str->type]);
            exit(1);
        }

        ListString_Append(list,args_str->stringValue->str);
    }
    
    Table(list_champ->stringValue,list);

    ListString_Clear(list);

    return self;
}