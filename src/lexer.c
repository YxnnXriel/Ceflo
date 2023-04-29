/**
 * @file lexer.c
 * @author Yann Ariel (yannariel13@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
    
#include "include/lexer.h"



static void lexer_skip_comment(lexer_t *lex){
    
    while(1 && lex->current_char != '\0'){
        if(lex->current_char == '\n'){
            lexer_advance(lex);
            break;
        }
        lexer_advance(lex);
    }    
}

static void lexer_skip_block_comment(lexer_t *lex){
    lexer_advance(lex);

    while (1 && lex->current_char != '\0'){
        if(lex->current_char == '*'){
            lexer_advance(lex);
            if(lex->current_char == '/'){
                lexer_advance(lex);
                break;
            }

        }
            
        lexer_advance(lex);
    }
}




/**
 * @brief new lexer
 * 
 * @param lexer 
 * @param source 
 */
void newLexer(lexer_t *lexer,const char* source){

    lexer->fileCode = readFile(source);
    lexer->lenght_file_code = strlen(lexer->fileCode);
    lexer->fileName = new_String(source);
    lexer->line = 1;
    lexer->colone = 1;
    lexer->index = 0;
    lexer->current_char = lexer->fileCode[lexer->index];

}

void newLexerWithString(lexer_t * lexer,const char *source){
    lexer->fileCode = calloc(strlen(source)+1,sizeof(char));
    strcpy(lexer->fileCode,source);
    lexer->lenght_file_code = strlen(lexer->fileCode);
    lexer->fileName = new_String(source);
    lexer->line = 1;
    lexer->colone = 1;
    lexer->index = 0;
    lexer->current_char = lexer->fileCode[lexer->index];
}








/**
 * @brief advance one caractere
 * 
 * @param lexer 
 */
void lexer_advance(lexer_t *lexer){
    if(lexer->current_char == 10 || lexer->current_char == '\0'){
        lexer->line +=1;
        lexer->colone = 0;
    }

    if(lexer->index < (int)lexer->lenght_file_code){
        lexer->colone +=1;
        lexer->index +=1;
        lexer->current_char = lexer->fileCode[lexer->index];
    }
}










/**
 * @brief precise what id is it
 * 
 * @param chaine 
 * @return TokenEnum 
 */
static TokenEnum lexer_precise_id(char *chaine ){

    #define cpm(value) strcmp(chaine,value) == 0


    if(cpm("for"))
        return TOKEN_FOR;
    if(cpm("while"))
        return TOKEN_WHILE;
    if(cpm("if"))
        return TOKEN_IF;
    if(cpm("else"))
        return TOKEN_ELSE;
    if(cpm("var"))
        return TOKEN_VAR;
    if(cpm("func"))
        return TOKEN_FUNC;
    if(cpm("lfunc"))
        return TOKEN_LFUNC;
    if(cpm("main"))
        return TOKEN_MAIN;
    if(cpm("return"))
        return TOKEN_RETURN;
    if(cpm("include"))
        return TOKEN_INCLUDE;
    if(cpm("struct"))
        return TOKEN_STRUCT;
    if(cpm("true"))
        return TOKEN_TRUE;
    if(cpm("false"))
        return TOKEN_FALSE;
    if(cpm("none"))
        return TOKEN_NONE;
    if(cpm("continue"))
        return TOKEN_CONTINUE;
    if(cpm("break"))
        return TOKEN_BREAK;
    if(cpm("or"))
        return TOKEN_OR;
    if(cpm("and"))
        return TOKEN_AND;
    if(cpm("pass"))
        return TOKEN_PASS;
    if(cpm("new"))
        return TOKEN_NEW;
    else
        return TOKEN_ID;

    #undef cpm
}








/**
 * @brief look on cararctere in the front
 * 
 * @param lexer 
 * @param c 
 * @return int 
 */
static int lexer_peek(lexer_t *lexer, char c){
    
    if(lexer->index + 1 < (int)lexer->lenght_file_code){

        if(lexer->fileCode[lexer->index + 1] == c) 
            return 1;
        return 0;

    }else{
        return 0;
    }

}








/**
 * @brief make value as string
 * 
 * @param is 
 * @param lexer 
 * @param value 
 * @return int 
 */
static char * lexer_get_value(lexer_t *lexer,int initiale ){
    size_t lenght = lexer->index - initiale + 1;
    char *value = calloc(lenght,sizeof(char));
    memcpy(value,&lexer->fileCode[initiale],lenght);
    value[lenght -1] = '\0';
    return value;
}









/**
 * @brief for identifier
 * 
 * @param lexer 
 * @return token_t 
 */
token_t lexer_collect_id(lexer_t *lexer){

    int initiale = lexer->index;

    while (isalnum(lexer->current_char) || lexer->current_char == '_'){
        lexer_advance(lexer);
    }

    char *value = lexer_get_value(lexer,initiale);
    int type = lexer_precise_id(value);
    return AS_TOKEN(type,value);
}








/**
 * @brief for number
 * 
 * @param lexer 
 * @return token_t 
 */
token_t  lexer_collect_digit(lexer_t *lexer){

    TokenEnum type = TOKEN_INTEGER;

    int initial = lexer->index;
    while (isdigit(lexer->current_char)){
        lexer_advance(lexer);
    }
    

    if(lexer->current_char == '.'){
        type = TOKEN_FLOAT;
        lexer_advance(lexer);
        while (isdigit(lexer->current_char))
            lexer_advance(lexer);
    }

    char *value = lexer_get_value(lexer,initial);

    return AS_TOKEN(type,value);
}








/**
 * @brief for strings
 * 
 * @param lexer 
 * @return token_t 
 */
token_t lexer_collect_string(lexer_t *lexer){


    lexer_advance(lexer);

    int initial = lexer->index;

    while (lexer->current_char != '"'){
        lexer_advance(lexer);
    }

    char *value = lexer_get_value(lexer,initial);
    lexer_advance(lexer);
    

    return AS_TOKEN(TOKEN_STRING,value);
}






/**
 * @brief for caractere
 * 
 * @param lexer 
 * @return token_t 
 */
token_t lexer_collect_char(lexer_t *lexer){
    lexer_advance(lexer);

    char *value = calloc(1,sizeof(char));
    value[0] = '\0';

    while (lexer->current_char != '\''){
        char *s = lexer_make_as_string(lexer);
        value = realloc(value , (strlen(s) + strlen(value) + 1) * sizeof(char));
        strcat(value,s);
        free(s);    
        lexer_advance(lexer);
    }

    lexer_advance(lexer);
    

    return AS_TOKEN(TOKEN_CHAR,value);
}








/**
 * @brief del white space
 * 
 * @param lexer 
 */
void lexer_advance_skip_white_space(lexer_t *lexer){
    while(lexer->current_char == 10 || lexer->current_char == ' ' || lexer->current_char == '\t')
        lexer_advance(lexer);
}







/**
 * @brief make sense of the current lexicon
 * 
 * @param lexer 
 * @return token_t 
 */
token_t lexer_get_next_token(lexer_t *lexer){


    while(lexer->current_char != '\0' && lexer->index < (int)lexer->lenght_file_code){


        if(lexer->current_char == 10  || lexer->current_char == ' ' || lexer->current_char == '\t')
            lexer_advance_skip_white_space(lexer);


        if(lexer->current_char == '/' && lexer_peek(lexer,'/'))
            lexer_skip_comment(lexer);
            

        if(lexer->current_char == '/' && lexer_peek(lexer,'*')){
            lexer_skip_block_comment(lexer);
            lexer_advance(lexer);
            continue;
        }


        if(isdigit(lexer->current_char))    
            return lexer_collect_digit(lexer);
        
        if(isalnum(lexer->current_char) || lexer->current_char == '_'){
            return lexer_collect_id(lexer);
        }

        if(lexer->current_char == '"')
            return lexer_collect_string(lexer);

        if(lexer->current_char == '\'')
            return lexer_collect_char(lexer);



        switch (lexer->current_char){
            
            case '(': return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_LPARENT,"("));
            case ')': return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_RPARENT,")"));
            case '{': return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_LBRACKET,"{"));
            case '}': return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_RBRACKET,"{"));
            case '[': return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_LBRACE,"["));
            case ']': return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_RBRACE,"]"));
            case '.': return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_DOT,"."));
            case ',': return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_COMA,","));
            case ';': return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_SEMI,";"));

            case ':':
                if(lexer_peek(lexer,':')){
                    lexer_advance(lexer);
                    return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_DOUBLE_COLON,"::"));
                }
                return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_COLON,":"));
            break;

            case '!':
                if(lexer_peek(lexer,'=')){
                    lexer_advance(lexer);
                    return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_DIF,"!="));
                }
                return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_NOT,"!"));
            break;


            case '=':
                if(lexer_peek(lexer,'=')){
                    lexer_advance(lexer);
                    return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_EQUAL_EQUAL,"=="));
                }
                return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_EQUAL,"="));
            break;

            case '+':
                if(lexer_peek(lexer,'=')){
                    lexer_advance(lexer);
                    return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_PLUS_EQUAL,"+="));
                }
                return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_PLUS,"+"));
            break;


            case '-':
                if(lexer_peek(lexer,'=')){
                    lexer_advance(lexer);
                    return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_MINUS_EQUAL,"-="));
                }else if (lexer_peek(lexer,'>')){
                    lexer_advance(lexer);
                    return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_ARROW,"->"));
                }
                return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_MINUS,"-"));
            break;

            case '*':
                if(lexer_peek(lexer,'=')){
                    lexer_advance(lexer);
                    return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_MULT_EQUAL,"*="));
                }
                return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_MULT,"*"));
            break;

            case '/':
                if(lexer_peek(lexer,'=')){
                    lexer_advance(lexer);
                    return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_DIV_EQUAL,"/="));
                }
                return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_DIV,"/"));
            break;

            case '<':
                if(lexer_peek(lexer,'=')){
                    lexer_advance(lexer);
                    return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_INF_EQUAL,"<="));
                }
                return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_INF,"<"));
            break;

            case '>':
                if(lexer_peek(lexer,'=')){
                    lexer_advance(lexer);
                    return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_SUP_EQUAL,">="));
                }
                return lexer_advance_return_token(lexer,AS_TOKEN(TOKEN_SUP,">"));
            break;
        }

        return AS_TOKEN(TOKEN_EOF,"\0");
    }
    
    return AS_TOKEN(TOKEN_EOF,"\0");
}





/**
 * @brief advance for lexer and return token
 * 
 * @param lexer 
 * @param token 
 * @return token_t 
 */
token_t lexer_advance_return_token(lexer_t *lexer , token_t token){

    lexer_advance(lexer);
    return token;
}






/**
 * @brief check if current char is a unicode
 * 
 * @param lex 
 * @return char 
 */
static char lexer_peek_retour(lexer_t *lex){
    if( (lex->index + 1) < (int)lex->lenght_file_code){
        if(lex->current_char == '\\' && lexer_peek(lex,'n')) return '\n';
        if(lex->current_char == '\\' && lexer_peek(lex,'a')) return '\a';
        if(lex->current_char == '\\' && lexer_peek(lex,'b')) return '\b';
        if(lex->current_char == '\\' && lexer_peek(lex,'t')) return '\t';
        if(lex->current_char == '\\' && lexer_peek(lex,'t')) return '\\';
        if(lex->current_char == '\\' && lexer_peek(lex,'"')) return '\"';
        if(lex->current_char == '\\' && lexer_peek(lex,'\'')) return '\'';


    }

    return -1;
}





/**
 * @brief transform char to string
 * 
 * @param lexer 
 * @return char* 
 */
char * lexer_make_as_string(lexer_t *lexer){
    char *chaine = calloc(2 ,sizeof(char*));

    char l = lexer_peek_retour(lexer);
    if(l != -1)
        lexer_advance(lexer);

    chaine[0] = l != -1 ? l:lexer->current_char;
    chaine[1] = '\0';

    return chaine;
}




/**
 * @brief clear lexer
 * 
 * @param lex 
 * @return int 
 */
int lexerFree(lexer_t *lex){

    if(lex->fileCode){
        free(lex->fileCode);
    }
    if(lex->fileName != NULL){
        String_free(lex->fileName);

    }

    return 1;
}