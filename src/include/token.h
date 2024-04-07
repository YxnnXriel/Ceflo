/**
 * @file token.h
 * @author your name (yannariel13@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-04-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */



                #ifndef __TOKEN_H__
                #define __TOKEN_H__

                #include "common.h"


typedef enum{
        TOKEN_ID, // value
        TOKEN_FOR, // for
        TOKEN_WHILE, // while
        TOKEN_IF, // if
        TOKEN_ELSE, // else
        TOKEN_VAR, // var
        TOKEN_CONST, // var
        TOKEN_FUNC, // func
        TOKEN_LFUNC , // lfunc
        TOKEN_MAIN , // main
        TOKEN_RETURN, // return
        TOKEN_IMPORT, // import
        TOKEN_FROM, // import
        TOKEN_EXPORT, // export
        TOKEN_STRUCT, // struct
        TOKEN_TRUE, // True
        TOKEN_FALSE, // False
        TOKEN_OR, // or
        TOKEN_AND, // and
        TOKEN_PASS , // pass
        TOKEN_NEW , // new
        TOKEN_CONTINUE, // continue
        TOKEN_GLOBAL, // global
        TOKEN_BREAK, // break
        TOKEN_NONE, // None
        TOKEN_CHAR , // 'char value'
        TOKEN_STRING, // "string value"
        TOKEN_INTEGER, // 123
        TOKEN_FLOAT, // 123.0
        TOKEN_AT_SIGN, // @
        TOKEN_NOT, // !
        TOKEN_DIF,// !=
        TOKEN_MODULO,// %
        TOKEN_ARROW, // ->
        TOKEN_COLON , // :
        TOKEN_DOUBLE_COLON , // ::
        TOKEN_DOT , // .
        TOKEN_SEMI, // ;
        TOKEN_MARK, // ?
        TOKEN_COMA, // , 
        TOKEN_LPARENT,  // (
        TOKEN_RPARENT,  // )
        TOKEN_LBRACKET, // {
        TOKEN_RBRACKET, // }
        TOKEN_LBRACE, // [
        TOKEN_RBRACE, // ]
        TOKEN_EQUAL,
        TOKEN_EQUAL_EQUAL,
        TOKEN_PLUS, // + 
        TOKEN_PLUS_EQUAL, // + 
        TOKEN_MINUS, // -
        TOKEN_MINUS_EQUAL, // -
        TOKEN_DIV, // /
        TOKEN_DIV_EQUAL, // /
        TOKEN_MULT, // *
        TOKEN_MULT_EQUAL, // *
        TOKEN_SUP, // >
        TOKEN_SUP_EQUAL , // >=
        TOKEN_INF , // <
        TOKEN_INF_EQUAL , // <=
        TOKEN_ADDR , // &
        TOKEN_EOF 
}TokenEnum;


typedef struct{
    TokenEnum type;
    char * value;
}token_t;

#define AS_TOKEN(ty,val) ((token_t){.type = ty,.value = val})



static const char *TOKEN_TO_STRING[]={
    "Identifiant",
    "for",
    "while",
    "if",
    "else",
    "var",
    "const",
    "func",
    "lfunc",
    "main",
    "return",
    "import",
    "from",
    "export",
    "struct",
    "true",
    "false",
    "or",
    "and",
    "pass",
    "new",
    "continue",
    "global",
    "break",
    "None",
    "char_value",
    "string_value",
    "integer_value",
    "float_value",
    "@",
    "!",
    "!=",
    "%%",
    "->",
    ":",
    "::",
    ".",
    ";",
    "?",
    ",",
    "(",
    ")",
    "{",
    "}",
    "[",
    "]",
    "=",
    "==",
    "+",
    "+=",
    "-",
    "-=",
    "/",
    "/=",
    "*",
    "*=",
    ">",
    ">=",
    "<",
    "<=",
    "&",
    "EOF"
};


token_t * newToken(TokenEnum type , const char *value);
token_t token_cpy(token_t tok);
int freeToken(token_t *token);




#endif