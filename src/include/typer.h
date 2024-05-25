#ifndef __TYPER_H__
#define __TYPER_H__


#include "visitor.h"
#include "ast.h"

ast_t * typer_caste_value( uint8_t to , ast_t *value);

ast_t *typer_typer(visitor_t *visitor ,const char *typer_name,list_ast_t *args);

ast_t * typer_caste_value_try_obj( uint8_t to , ast_t **value , ast_t *obj);


ast_t * typer_value_expr(ast_t * left , token_t * op , ast_t * right);



#endif