/**
 * @file parser.c
 * @author Yann Ariel (yannariel13@gmail.com)
 * @brief analyse descendante : une fonction par règle de GRAMMAIRE.md
 * @copyright Copyright (c) 2023
 */

#include <stdlib.h>
#include <string.h>

#include "include/parser.h"
#include "include/error.h"
#include "include/str.h"

#define TYPE(parser) ((parser)->current.type)
#define VALUE(parser) ((parser)->current.value)

static ast_t *parser_parse_expr(parser_t *parser);


//                          ===================================== $OUTILS ==============================================


static _Noreturn void parser_error_expected(parser_t *parser, const char *expected){
    error_syntax(
        parser->lexer->fileName,
        parser->current.line,
        parser->current.colone,
        "`%s` attendu, mais `%s` trouvé",
        expected,
        token_to_string(TYPE(parser))
    );
}

static _Noreturn void parser_error_here(parser_t *parser, const char *message){
    error_syntax(parser->lexer->fileName,parser->current.line,parser->current.colone,"%s",message);
}

static void newParser(parser_t *parser, lexer_t *lexer){
    memset(parser,0,sizeof(*parser));
    parser->lexer = lexer;
    parser->prev.type = TOKEN_EOF;
    parser->current = lexer_get_next_token(lexer);
}

static void freeParser(parser_t *parser){
    freeToken(&parser->prev);
    freeToken(&parser->current);
}

/**
 * @brief passe au jeton suivant : l'ancien jeton reste lisible dans parser->prev
 */
static void parser_advance(parser_t *parser){
    freeToken(&parser->prev);
    parser->prev = parser->current;
    parser->current = lexer_get_next_token(parser->lexer);
}

static void parser_eat(parser_t *parser, TokenEnum type){
    if(TYPE(parser) != type)
        parser_error_expected(parser,token_to_string(type));
    parser_advance(parser);
}


static ast_t *parser_new_node(parser_t *parser, ast_type type){
    return newAstWithLine(type,parser->lexer->fileName,parser->current.line,parser->current.colone);
}

static void parser_check_depth(parser_t *parser){
    if(ceflo_stack_exceeded())
        parser_error_here(parser,"code trop profondément imbriqué");
}

/**
 * @brief lit un nom et en donne une copie
 */
static char *parser_eat_id(parser_t *parser){
    char *name = NULL;
    if(TYPE(parser) != TOKEN_ID)
        parser_error_expected(parser,"identifiant");
    allowChar(&name,VALUE(parser));
    parser_advance(parser);
    return name;
}

static ast_t *parser_parse_number(parser_t *parser){
    ast_t *number = parser_new_node(parser,AST_NUMBER);
    number->number_value = strtod(VALUE(parser),NULL);
    parser_advance(parser);
    return number;
}

static ast_t *parser_parse_primary(parser_t *parser){
    ast_t *node = NULL;

    switch(TYPE(parser)){
        case TOKEN_INTEGER:
        case TOKEN_FLOAT:
            return parser_parse_number(parser);
        case TOKEN_ID:
            node = parser_new_node(parser,AST_VARIABLE);
            node->variable_name = parser_eat_id(parser);
            return node;
        case TOKEN_LPARENT:
            parser_advance(parser);
            node = parser_parse_expr(parser);
            parser_eat(parser,TOKEN_RPARENT);
            return node;
        default:
            parser_error_expected(parser,"expression");
    }
}

// Étape 1 : pas encore de suffixes. On les écrira à l'étape 2.
static ast_t *parser_parse_postfix(parser_t *parser){
    return parser_parse_primary(parser);
}

static ast_t *parser_parse_factor(parser_t *parser){
    parser_check_depth(parser);

    if(TYPE(parser) == TOKEN_MINUS || TYPE(parser) == TOKEN_NOT){
        ast_t *unop = parser_new_node(parser,AST_UNOP);
        unop->op = TYPE(parser);
        parser_advance(parser);
        unop->right = parser_parse_factor(parser);
        return unop;
    }

    return parser_parse_postfix(parser);
}

static ast_t *parser_new_binop(parser_t *parser, ast_t *left){
    ast_t *binop = parser_new_node(parser,AST_BINOP);
    binop->left = left;
    binop->op = TYPE(parser);
    parser_advance(parser);
    return binop;
}

static ast_t *parser_parse_term(parser_t *parser){
    ast_t *value = parser_parse_factor(parser);

    while(TYPE(parser) == TOKEN_MULT || TYPE(parser) == TOKEN_DIV || TYPE(parser) == TOKEN_MODULO){
        ast_t *binop = parser_new_binop(parser,value);
        binop->right = parser_parse_factor(parser);
        value = binop;
    }

    return value;
}

static ast_t *parser_parse_expression(parser_t *parser){
    ast_t *value = parser_parse_term(parser);

    while(TYPE(parser) == TOKEN_PLUS || TYPE(parser) == TOKEN_MINUS){
        ast_t *binop = parser_new_binop(parser,value);
        binop->right = parser_parse_term(parser);
        value = binop;
    }

    return value;
}

// Étape 1 : une expression, c'est une somme. Les étages du dessus viendront à l'étape 2.
static ast_t *parser_parse_expr(parser_t *parser){
    return parser_parse_expression(parser);
}

// Étapes 1 et 2 : un programme est une suite d'expressions.
ast_t *parser_parse(lexer_t *lexer){
    parser_t parser;
    newParser(&parser,lexer);

    ast_t *block = parser_new_node(&parser,AST_COMPOUND);
    block->list_compound = newList();
    while(TYPE(&parser) != TOKEN_EOF)
        List_Append(block->list_compound,parser_parse_expr(&parser));

    freeParser(&parser);
    return block;
}
