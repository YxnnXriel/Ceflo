#ifndef __PARSER_H__
#define __PARSER_H__

    #include <stdarg.h>


#include "lexer.h"
#include "token.h"
#include "ast.h"
#include "scope.h"
#include "error.h"

typedef struct{
    lexer_t *lexer;
    token_t current;
    token_t prev;
}parser_t;


void newParser(parser_t *parser,lexer_t *lexer);
int parser_eat(TokenEnum type,errorE errorType,char *fmt , ...);


ast_t *parser_parse(lexer_t *lexer);

ast_t *parser_parse_statment(parser_t *parser , scope_t * scope);
ast_t *parser_parse_statments(parser_t *parser , scope_t * scope);
ast_t *parser_parse_compound(parser_t *parser , scope_t * scope);

ast_t *parser_parse_variable_definition(parser_t *parser , scope_t * scope);
ast_t *parser_parse_variable(parser_t *parser , scope_t * scope);
ast_t *parser_parse_function_definition(parser_t *parser , scope_t * scope);
ast_t *parser_parse_function_call(parser_t *parser , scope_t * scope);

ast_t *parser_parse_number(parser_t *parser , scope_t * scope);
ast_t *parser_parse_char(parser_t *parser , scope_t * scope);
ast_t *parser_parse_bool(parser_t *parser , scope_t * scope);
ast_t *parser_parse_string(parser_t *parser , scope_t * scope);

ast_t *parser_parse_expr(parser_t *parser , scope_t *scope);
ast_t *parser_parse_expression(parser_t *parser , scope_t *scope);
ast_t *parser_parse_term(parser_t *parser , scope_t *scope);
ast_t *parser_parse_factor(parser_t *parser , scope_t *scope);

ast_t *parser_parse_return(parser_t *parser , scope_t *scope);

ast_t *parser_parse_if(parser_t *parser , scope_t *scope);
ast_t *parser_parse_while(parser_t *parser , scope_t *scope);
ast_t *parser_parse_break(parser_t *parser , scope_t *scope);




#endif