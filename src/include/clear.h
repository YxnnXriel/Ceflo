#ifndef __CLEAR_H__
#define __CLEAR_H__

    #include "lexer.h"
    #include "visitor.h"


struct clear_ceflo {
    list_t *visitor_list;
    ast_t *first_ast;    
};




void Clear_Ceflo(lexer_t * lexer);

void Clear_Ceflo_Parser(lexer_t * lexer);

void clear_ceflo_push(visitor_t *visitor);

void clear_ceflo_clean();





#endif