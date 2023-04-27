#ifndef __CLEAR_H__
#define __CLEAR_H__

    #include "lexer.h"
    #include "visitor.h"

void Clear_Ceflo(struct VISITOR_STRUCT * visitor , lexer_t * lexer);

void Clear_Ceflo_Parser(lexer_t * lexer);


#endif