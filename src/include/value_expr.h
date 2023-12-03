#ifndef __VALUE_EXPR_H__
#define __VALUE_EXPR_H__

#include "ast.h"
#include "token.h"


void performe_visit_tree_char(Value_t * value, Value_t * left , token_t * op, Value_t * right);
void performe_visit_tree_number(Value_t * value, Value_t * left , token_t * op, Value_t * right);
void performe_visit_tree_bool(Value_t * value, Value_t * left , token_t * op, Value_t * right);
void performe_visit_tree_str(Value_t * value, Value_t * left , token_t * op, Value_t * right);
void performe_visit_tree_none(Value_t * value, Value_t * left , token_t * op, Value_t * right);
int value_to_type_ast(Value_t *value);
int type_ast_to_value(ast_t * node_value);



#endif