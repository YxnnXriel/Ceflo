/**
 * @file lexer.c
 * @author Yann Ariel (yannariel13@gmail.com)
 * @brief transforme le texte du fichier source en jetons
 * @copyright Copyright (c) 2023
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "include/lexer.h"
#include "include/error.h"
#include "include/str.h"

static void lexer_init(lexer_t *lexer, const char *name, char *code){
    lexer->fileName = error_add_source(name,code);
    lexer->fileCode = code;
    lexer->lenght_file_code = strlen(code);
    lexer->line = 1;
    lexer->colone = 1;
    lexer->index = 0;
    lexer->current_char = code[0];
}

/**
 * @brief ouvre un fichier source
 */
bool newLexer(lexer_t *lexer, const char *path){
    char *code = readFile(path);
    if(code == NULL)
        return false;

    lexer_init(lexer,path,code);

    // Un script peut commencer par #!/usr/bin/env Ceflo
    if(code[0] == '#' && code[1] == '!'){
        while(lexer->current_char != '\n' && lexer->current_char != '\0')
            lexer_advance(lexer);
    }

    return true;
}

/**
 * @brief lit directement une chaîne, pour les expressions de format()
 */
void newLexerWithString(lexer_t *lexer, const char *name, const char *source){
    char *code = NULL;
    allowChar(&code,source);
    lexer_init(lexer,name,code);
}

void lexer_advance(lexer_t *lexer){
    if(lexer->index >= (int)lexer->lenght_file_code)
        return;

    if(lexer->current_char == '\n'){
        lexer->line += 1;
        lexer->colone = 1;
    }else{
        lexer->colone += 1;
    }

    lexer->index += 1;
    lexer->current_char = lexer->fileCode[lexer->index];
}

static char lexer_peek(lexer_t *lexer){
    if(lexer->index + 1 >= (int)lexer->lenght_file_code)
        return '\0';
    return lexer->fileCode[lexer->index + 1];
}

static bool lexer_at_end(lexer_t *lexer){
    return lexer->index >= (int)lexer->lenght_file_code;
}

// Étape 1 : seulement les espaces et les retours à la ligne. Les commentaires viendront à l'étape 2.
static bool lexer_skip_ignored(lexer_t *lexer){
    bool newline = false;

    while(!lexer_at_end(lexer)){
        char carac = lexer->current_char;

        if(carac == '\n'){
            newline = true;
            lexer_advance(lexer);
        }else if(carac == ' ' || carac == '\t' || carac == '\r'){
            lexer_advance(lexer);
        }else{
            break;
        }
    }

    return newline;
}

static token_t lexer_make_token(lexer_t *lexer, TokenEnum type, int size){
    token_t token = {
        .type = type,
        .value = (char*)token_to_string(type),
        .line = lexer->line,
        .colone = lexer->colone
    };
    for(int i = 0; i < size; i++)
        lexer_advance(lexer);
    return token;
}

// Symbole qui peut être suivi d'un second caractère : `=` ou `==`, `+` ou `+=`...
static token_t lexer_check_token(lexer_t *lexer, char next, TokenEnum type_next, TokenEnum type_alone){
    if(lexer_peek(lexer) == next)
        return lexer_make_token(lexer,type_next,2);
    return lexer_make_token(lexer,type_alone,1);
}

/**
 * @brief donne le jeton suivant
 */
// Étape 1 : la fin du fichier et les symboles. Les nombres, les noms et les chaînes viendront ensuite.
static token_t lexer_scan_token(lexer_t *lexer){
    char carac = lexer->current_char;

    if(lexer_at_end(lexer))
        return lexer_make_token(lexer,TOKEN_EOF,0);

    switch(carac){
        case '(': return lexer_make_token(lexer,TOKEN_LPARENT,1);
        case ')': return lexer_make_token(lexer,TOKEN_RPARENT,1);
        case '{': return lexer_make_token(lexer,TOKEN_LBRACKET,1);
        case '}': return lexer_make_token(lexer,TOKEN_RBRACKET,1);
        case '[': return lexer_make_token(lexer,TOKEN_LBRACE,1);
        case ']': return lexer_make_token(lexer,TOKEN_RBRACE,1);
        case '.': return lexer_make_token(lexer,TOKEN_DOT,1);
        case ',': return lexer_make_token(lexer,TOKEN_COMA,1);
        case ';': return lexer_make_token(lexer,TOKEN_SEMI,1);
        case '@': return lexer_make_token(lexer,TOKEN_AT_SIGN,1);
        case '?': return lexer_make_token(lexer,TOKEN_MARK,1);
        case ':': return lexer_check_token(lexer,':',TOKEN_DOUBLE_COLON,TOKEN_COLON);
        case '!': return lexer_check_token(lexer,'=',TOKEN_DIF,TOKEN_NOT);
        case '=': return lexer_check_token(lexer,'=',TOKEN_EQUAL_EQUAL,TOKEN_EQUAL);
        case '+': return lexer_check_token(lexer,'=',TOKEN_PLUS_EQUAL,TOKEN_PLUS);
        case '*': return lexer_check_token(lexer,'=',TOKEN_MULT_EQUAL,TOKEN_MULT);
        case '/': return lexer_check_token(lexer,'=',TOKEN_DIV_EQUAL,TOKEN_DIV);
        case '%': return lexer_check_token(lexer,'=',TOKEN_MODULO_EQUAL,TOKEN_MODULO);
        case '<': return lexer_check_token(lexer,'=',TOKEN_INF_EQUAL,TOKEN_INF);
        case '>': return lexer_check_token(lexer,'=',TOKEN_SUP_EQUAL,TOKEN_SUP);
        case '-':
            if(lexer_peek(lexer) == '>')
                return lexer_make_token(lexer,TOKEN_ARROW,2);
            return lexer_check_token(lexer,'=',TOKEN_MINUS_EQUAL,TOKEN_MINUS);
        default:
            break;
    }

    // Un caractère accentué occupe plusieurs octets : on les affiche tous.
    int size = 1;
    while(((unsigned char)lexer->fileCode[lexer->index + size] & 0xC0) == 0x80)
        size += 1;

    error_syntax(lexer->fileName,lexer->line,lexer->colone,
        "caractère inattendu `%.*s`",size,&lexer->fileCode[lexer->index]);
}

token_t lexer_get_next_token(lexer_t *lexer){
    bool newline = lexer_skip_ignored(lexer);
    token_t token = lexer_scan_token(lexer);
    token.newline_before = newline;
    return token;
}
