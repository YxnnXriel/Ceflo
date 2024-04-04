#include <stdio.h>
#include <stdlib.h>

#include <math.h>
#include "../../src/include/ast.h"
#include "../../src/include/visitor.h"

struct AST_STRUCT *mathPi(struct VISITOR_STRUCT *vs ,struct AST_STRUCT*node ,struct LIST_AST_STRUCT *args){

    ast_t *value = newAst(AST_NUMBER);
    value->value.as.numberValue = 3.14;
    value->value.type = NUMBER;
    return value;
}


struct AST_STRUCT *mathLog10(struct VISITOR_STRUCT *vs ,struct AST_STRUCT*node ,struct LIST_AST_STRUCT *args){

    ast_t * args1 = List_Ast_Get(args,0);

    args1 = visitor_visit(vs,args1);

    ast_t *value = newAst(AST_NUMBER);
    value->value.as.numberValue = log10(args1->value.as.numberValue);
    value->value.type = NUMBER;

    return value;
}
