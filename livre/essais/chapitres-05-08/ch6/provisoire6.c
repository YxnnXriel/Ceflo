// Le provisoire du chapitre 5, plus ce que typer.c demande au visitor.
#include "visitor.h"
#include "error.h"

value_t *visitor_new_object(visitor_t *visitor, ast_t *node, value_t *structure, value_t **args, int argc){
    (void)visitor;(void)structure;(void)args;(void)argc;
    visitor_error(node,"pas encore de structures");
}
