/**
 * @file parser.h
 * @author Yann Ariel (yannariel13@gmail.com)
 * @brief la grammaire complète est dans GRAMMAIRE.md : chaque règle y correspond
 *        à une fonction parser_parse_*
 * @copyright Copyright (c) 2023
 */

#ifndef __PARSER_H__
#define __PARSER_H__

    #include "lexer.h"
    #include "token.h"
    #include "ast.h"

    typedef struct{
        lexer_t *lexer;
        token_t current;
        token_t prev;
        int function_depth;   // pour refuser `return` hors d'une fonction
        int loop_depth;       // pour refuser `break` hors d'une boucle
        int block_depth;      // pour refuser `export` dans un bloc
    }parser_t;

    // Analyse tout un fichier.
    ast_t *parser_parse(lexer_t *lexer);

    // Analyse une seule expression, pour les ${...} de format().
    ast_t *parser_parse_one(lexer_t *lexer);

#endif
