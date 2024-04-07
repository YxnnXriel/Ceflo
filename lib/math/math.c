#include <stdio.h>
#include <stdlib.h>

#include <math.h>
#include "../../src/include/ast.h"
#include "../../src/include/visitor.h"

struct AST_STRUCT *mathPi(struct VISITOR_STRUCT *vs ,struct AST_STRUCT*node ,struct LIST_AST_STRUCT *args){
    
    return makeNumber(acos(-1.0));
}


struct AST_STRUCT *mathLog10(struct VISITOR_STRUCT *vs ,struct AST_STRUCT*node ,struct LIST_AST_STRUCT *args){

    ast_t * args1 = List_Ast_Get(args,0);

    args1 = visitor_visit(vs,args1);
    
    double value = 0;
    if(AS_NUMBER(args1->value) != 0)
        value = log10(args1->value.as.numberValue);

    ast_t * val = makeNumber(value);

    return val;
}
