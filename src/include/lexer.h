/**
 * @file lexer.h
 * @author Yann Ariel (yannariel13@gmail.com)
 * @copyright Copyright (c) 2023
 */

#ifndef __LEXER_H__
#define __LEXER_H__

#include "token.h"
#include "common.h"

typedef struct{
    const char *fileName;
    const char *fileCode;
    size_t lenght_file_code;
    int index;
    char current_char;
    int line;
    int colone;
}lexer_t;

// Retourne false si le fichier ne peut pas être lu.
bool newLexer(lexer_t *lexer, const char *path);
void newLexerWithString(lexer_t *lexer, const char *name, const char *source);

void lexer_advance(lexer_t *lexer);
token_t lexer_get_next_token(lexer_t *lexer);

#endif
