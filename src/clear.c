#include <stdio.h>
#include <stdlib.h>
#include "include/clear.h"



void Clear_Ceflo(visitor_t * visitor , lexer_t * lexer){
    
    while (visitor->global_scope->scope_parent != NULL){
	    Clear_Ceflo(visitor->global_scope->scope_parent,NULL);
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