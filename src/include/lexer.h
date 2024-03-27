/**
 * @file lexer.h
 * @author Yann Ariel (yannariel13@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */

                #ifndef __LEXER_H__
                #define __LEXER_H__


                        #include "str.h"
                        #include "token.h"

    typedef struct{
        char *fileCode;
        size_t lenght_file_code;
        int index;
        char current_char;
        int line;
        int colone;
        string_t *fileName;
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
void newLexerWithString(lexer_t * lexer,const char *source);
char * lexer_get_line_for_file(lexer_t * lexer , int line);



token_t lexer_check_return_token(lexer_t *lexer,char carac_peek,char *default_value, 
    char *peek_value, uint8_t type_peek ,uint8_t type_default);


#endif