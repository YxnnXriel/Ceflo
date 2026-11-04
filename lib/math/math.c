#include <math.h>

#include "visitor.h"

value_t *mathPi(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"mathPi",argc,0);
    return makeNumber(acos(-1.0));
}

value_t *mathLog10(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"mathLog10",argc,1);
    Value_check_type(node,"mathLog10",args[0],NUMBER);
    return makeNumber(log10(AS_NUMBER(args[0])));
}

value_t *mathSqrt(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"mathSqrt",argc,1);
    Value_check_type(node,"mathSqrt",args[0],NUMBER);
    return makeNumber(sqrt(AS_NUMBER(args[0])));
}
