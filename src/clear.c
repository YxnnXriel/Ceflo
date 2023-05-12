#include <stdio.h>
#include <stdlib.h>
#include "include/clear.h"




static struct clear_ceflo myclear;

void Clear_Ceflo(lexer_t * lexer){

    List_Del_all(
        myclear.visitor_list,
        freeLvisitor
    );

    
    // if(myclear.visitor_list != NULL)
    //     free(myclear.visitor_list);

    Clear_Ceflo_Parser(lexer);    
}

void Clear_Ceflo_Parser(lexer_t * lexer){
    clearAst();

	lexerFree(lexer);
}

void clear_ceflo_push(visitor_t *visitor){
    
    if(myclear.visitor_list == NULL){
        myclear.visitor_list = __init__list__(sizeof(visitor_t));
        // printf("%s \n",myclear.visitor_list == NULL ? "OUI":"NON");
    }

    List_Append(
        myclear.visitor_list,
        visitor
    );
}

void clear_ceflo_clean(){

}