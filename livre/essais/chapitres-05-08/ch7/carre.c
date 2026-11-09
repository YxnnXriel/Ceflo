// Une fonction écrite en C pour Ceflo_ref : elle a la forme de ceflo_func_t (src/include/Value.h).
#include "Value.h"
#include "visitor.h"

value_t *carre(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"carre",argc,1);
    Value_check_type(node,"carre",args[0],NUMBER);
    return makeNumber(AS_NUMBER(args[0]) * AS_NUMBER(args[0]));
}
