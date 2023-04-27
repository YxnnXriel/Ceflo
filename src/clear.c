#include <stdio.h>
#include <stdlib.h>
#include "include/clear.h"



void Clear_Ceflo(struct VISITOR_STRUCT * visitor , lexer_t * lexer){
    
    while (visitor->global_scope->scope_parent != NULL){
        visitor_t vs;
        vs.global_scope = visitor->global_scope->scope_parent;
	    Clear_Ceflo(&vs,NULL);
    }
    if(visitor->global_scope->countScope != 0)
	    freeScope(visitor->global_scope);

    if(lexer != NULL){
        clearAst();
	    freeScope(visitor->global_scope);
	    lexerFree(lexer);
    }
}

void Clear_Ceflo_Parser(lexer_t * lexer){
    clearAst();
	lexerFree(lexer);
}