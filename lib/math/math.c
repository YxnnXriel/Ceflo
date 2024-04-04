#include <stdio.h>
#include <stdlib.h>

#include <math.h>
#include "../../src/include/ast.h"


struct AST_STRUCT *Pi(struct VISITOR_STRUCT *vs ,struct AST_STRUCT*node ,struct LIST_AST_STRUCT *args){

    ast_t *value = newAst(AST_NUMBER);
    value->value.as.numberValue = 3.14;
    value->value.type = NUMBER;
    return value;
}