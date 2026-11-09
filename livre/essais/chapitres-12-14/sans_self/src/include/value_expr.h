#ifndef __VALUE_EXPR_H__
#define __VALUE_EXPR_H__

    #include "visitor.h"

    // Calcule `left op right`. `node` situe l'erreur si l'opération est impossible.
    value_t *value_expr(visitor_t *visitor, ast_t *node, value_t *left, TokenEnum op, value_t *right);

#endif
