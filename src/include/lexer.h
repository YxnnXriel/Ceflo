#ifndef __LEXER_H__
#define __LEXER_H__


#include "str.h"
#include "token.h"

typedef struct{
    string_t *fileCode;
    // string_t *fileName;
    int index;
    char current_char;
    int line;
    int colone;
}lexer_t;

void newLexer(lexer_t *lexer,const char* source);

void lexer_advance(lexer_t *lexer);

void lexer_advance_skip_white_space(lexer_t *lexer);

token_t lexer_collect_id(lexer_t *lexer);
token_t lexer_collect_digit(lexer_t *lexer);
token_t lexer_collect_string(lexer_t *lexer);
token_t lexer_collect_char(lexer_t *lexer);
token_t lexer_get_next_token(lexer_t *lexer);
token_t lexer_advance_return_token(lexer_t *lexer , token_t token);

char * lexer_make_as_string(lexer_t *lexer);


int lexerFree(lexer_t *lexer);





#endif