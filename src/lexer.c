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

/**
 * @brief saute les espaces et les commentaires
 *
 * @return true si un retour à la ligne a été franchi
 */
static bool lexer_skip_ignored(lexer_t *lexer){
    bool newline = false;

    while(!lexer_at_end(lexer)){
        char carac = lexer->current_char;

        if(carac == '\n'){
            newline = true;
            lexer_advance(lexer);
        }else if(carac == ' ' || carac == '\t' || carac == '\r'){
            lexer_advance(lexer);
        }else if(carac == '/' && lexer_peek(lexer) == '/'){
            while(!lexer_at_end(lexer) && lexer->current_char != '\n')
                lexer_advance(lexer);
        }else if(carac == '/' && lexer_peek(lexer) == '*'){
            int line = lexer->line;
            int colone = lexer->colone;
            lexer_advance(lexer);
            lexer_advance(lexer);
            while(!(lexer->current_char == '*' && lexer_peek(lexer) == '/')){
                if(lexer_at_end(lexer))
                    error_syntax(lexer->fileName,line,colone,"commentaire `/*` jamais fermé");
                if(lexer->current_char == '\n')
                    newline = true;
                lexer_advance(lexer);
            }
            lexer_advance(lexer);
            lexer_advance(lexer);
        }else{
            break;
        }
    }

    return newline;
}

static TokenEnum lexer_precise_id(const char *chaine){
    static const struct { const char *name; TokenEnum type; } keywords[] = {
        {"for",TOKEN_FOR},
        {"in",TOKEN_IN},
        {"while",TOKEN_WHILE},
        {"if",TOKEN_IF},
        {"else",TOKEN_ELSE},
        {"var",TOKEN_VAR},
        {"const",TOKEN_CONST},
        {"func",TOKEN_FUNC},
        {"lfunc",TOKEN_LFUNC},
        {"return",TOKEN_RETURN},
        {"import",TOKEN_IMPORT},
        {"from",TOKEN_FROM},
        {"export",TOKEN_EXPORT},
        {"struct",TOKEN_STRUCT},
        {"true",TOKEN_TRUE},
        {"false",TOKEN_FALSE},
        {"none",TOKEN_NONE},
        {"or",TOKEN_OR},
        {"and",TOKEN_AND},
        {"new",TOKEN_NEW},
        {"continue",TOKEN_CONTINUE},
        {"break",TOKEN_BREAK},
    };

    for(size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++){
        if(strcmp(chaine,keywords[i].name) == 0)
            return keywords[i].type;
    }

    return TOKEN_ID;
}

static char *lexer_get_value(lexer_t *lexer, int initiale){
    int lenght = lexer->index - initiale;
    char *value = ceflo_alloc((size_t)lenght + 1);
    memcpy(value,&lexer->fileCode[initiale],(size_t)lenght);
    return value;
}

static token_t lexer_collect_id(lexer_t *lexer){
    token_t token = {.line = lexer->line, .colone = lexer->colone};
    int initiale = lexer->index;

    while(isalnum((unsigned char)lexer->current_char) || lexer->current_char == '_')
        lexer_advance(lexer);

    char *value = lexer_get_value(lexer,initiale);
    token.type = lexer_precise_id(value);

    if(token_owns_value(token.type)){
        token.value = value;
    }else{
        free(value);
        token.value = (char*)token_to_string(token.type);
    }
    return token;
}

static token_t lexer_collect_digit(lexer_t *lexer){
    token_t token = {.type = TOKEN_INTEGER, .line = lexer->line, .colone = lexer->colone};
    int initial = lexer->index;

    while(isdigit((unsigned char)lexer->current_char))
        lexer_advance(lexer);

    if(lexer->current_char == '.' && isdigit((unsigned char)lexer_peek(lexer))){
        token.type = TOKEN_FLOAT;
        lexer_advance(lexer);
        while(isdigit((unsigned char)lexer->current_char))
            lexer_advance(lexer);
    }

    if(isalpha((unsigned char)lexer->current_char) || lexer->current_char == '_')
        error_syntax(lexer->fileName,token.line,token.colone,"nombre mal formé");

    token.value = lexer_get_value(lexer,initial);
    return token;
}

/**
 * @brief lit un caractère de chaîne, en traduisant les séquences comme \n
 */
static void lexer_collect_escape(lexer_t *lexer, buffer_t *buffer){
    if(lexer->current_char != '\\'){
        Buffer_Append_Char(buffer,lexer->current_char);
        lexer_advance(lexer);
        return;
    }

    lexer_advance(lexer);
    switch(lexer->current_char){
        case 'n': Buffer_Append_Char(buffer,'\n'); break;
        case 't': Buffer_Append_Char(buffer,'\t'); break;
        case 'r': Buffer_Append_Char(buffer,'\r'); break;
        case 'a': Buffer_Append_Char(buffer,'\a'); break;
        case 'b': Buffer_Append_Char(buffer,'\b'); break;
        case '0': Buffer_Append_Char(buffer,'\0'); break;
        case '\\': Buffer_Append_Char(buffer,'\\'); break;
        case '"': Buffer_Append_Char(buffer,'"'); break;
        case '\'': Buffer_Append_Char(buffer,'\''); break;
        default:
            // Séquence inconnue : on la garde telle quelle (format() utilise \$).
            Buffer_Append_Char(buffer,'\\');
            Buffer_Append_Char(buffer,lexer->current_char);
            break;
    }
    lexer_advance(lexer);
}

static token_t lexer_collect_quoted(lexer_t *lexer, char quote, TokenEnum type){
    token_t token = {.type = type, .line = lexer->line, .colone = lexer->colone};
    buffer_t buffer;
    newBuffer(&buffer);

    lexer_advance(lexer);
    while(lexer->current_char != quote){
        if(lexer_at_end(lexer)){
            error_syntax(lexer->fileName,token.line,token.colone,
                type == TOKEN_STRING ? "chaîne jamais fermée" : "caractère jamais fermé");
        }
        lexer_collect_escape(lexer,&buffer);
    }
    lexer_advance(lexer);

    if(type == TOKEN_CHAR && buffer.size != 1){
        error_syntax(lexer->fileName,token.line,token.colone,
            "un caractère contient un seul octet, utilisez \"...\" pour une chaîne");
    }

    token.value = buffer.data;
    token.lenght = (int)buffer.size;
    return token;
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
static token_t lexer_scan_token(lexer_t *lexer){
    char carac = lexer->current_char;

    if(lexer_at_end(lexer))
        return lexer_make_token(lexer,TOKEN_EOF,0);
    if(isdigit((unsigned char)carac))
        return lexer_collect_digit(lexer);
    if(isalpha((unsigned char)carac) || carac == '_')
        return lexer_collect_id(lexer);
    if(carac == '"')
        return lexer_collect_quoted(lexer,'"',TOKEN_STRING);
    if(carac == '\'')
        return lexer_collect_quoted(lexer,'\'',TOKEN_CHAR);

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
