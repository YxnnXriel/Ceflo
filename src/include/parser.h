/**
 * @file parser.h
 * @author Yann Ariel (yannariel13@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-10
 * 
 * @copyright Copyright (c) 2023
 * 
 */

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
void parser_error(parser_t * parser );
void freeParser(parser_t * parser);

int parser_eat(TokenEnum type, parser_t * parser, char *fmt , ...);


ast_t *parser_parse(lexer_t *lexer);

ast_t *parser_parse_statment(parser_t *parser);
ast_t *parser_parse_statments(parser_t *parser);
ast_t *parser_parse_compound(parser_t *parser);



ast_t *parser_parse_variable_definition(parser_t *parser);
ast_t *parser_parse_variable(parser_t *parser);

ast_t *parser_parse_function_definition(parser_t *parser);
ast_t *parser_parse_function_call(parser_t *parser);
ast_t *parser_parse_lfunc(parser_t *parser);
ast_t *parser_parse_function_call_With_Name(ast_t * prev,parser_t *parser);

ast_t *parser_parse_number(parser_t *parser);
ast_t *parser_parse_char(parser_t *parser);
ast_t *parser_parse_bool(parser_t *parser);
ast_t *parser_parse_string(parser_t *parser);

ast_t *parser_parse_expr(parser_t *parser);
ast_t *parser_parse_expression(parser_t *parser);
ast_t *parser_parse_term(parser_t *parser);
ast_t *parser_parse_factor(parser_t *parser);

ast_t *parser_parse_return(parser_t *parser);

ast_t *parser_parse_if(parser_t *parser);
ast_t *parser_parse_while(parser_t *parser);
ast_t *parser_parse_break(parser_t *parser);

ast_t *parser_parse_list(parser_t *parser);
ast_t *parser_parse_struct_definition(parser_t *parser);




#endif