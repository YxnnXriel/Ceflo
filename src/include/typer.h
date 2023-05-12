#ifndef __TYPER_H__
#define __TYPER_H__


#include "visitor.h"
#include "ast.h"

ast_t * typer_caste_value( uint8_t to , ast_t *value);

ast_t *typer_typer(visitor_t *visitor ,const char *typer_name,list_ast_t *args);

#endif