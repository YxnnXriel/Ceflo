/**
 * @file token.h
 * @author Yann Ariel (yannariel13@gmail.com)
 * @copyright Copyright (c) 2023
 */

#ifndef __TOKEN_H__
#define __TOKEN_H__

#include "common.h"

#define FOREACH_TOKEN(TOKEN)                 \
    TOKEN(TOKEN_ID, "identifiant")           \
    TOKEN(TOKEN_INTEGER, "nombre")           \
    TOKEN(TOKEN_FLOAT, "nombre")             \
    TOKEN(TOKEN_CHAR, "caractère")           \
    TOKEN(TOKEN_STRING, "chaîne")            \
    TOKEN(TOKEN_FOR, "for")                  \
    TOKEN(TOKEN_IN, "in")                    \
    TOKEN(TOKEN_WHILE, "while")              \
    TOKEN(TOKEN_IF, "if")                    \
    TOKEN(TOKEN_ELSE, "else")                \
    TOKEN(TOKEN_MATCH, "match")              \
    TOKEN(TOKEN_CASE, "case")                \
    TOKEN(TOKEN_VAR, "var")                  \
    TOKEN(TOKEN_CONST, "const")              \
    TOKEN(TOKEN_FUNC, "func")                \
    TOKEN(TOKEN_LFUNC, "lfunc")              \
    TOKEN(TOKEN_RETURN, "return")            \
    TOKEN(TOKEN_IMPORT, "import")            \
    TOKEN(TOKEN_FROM, "from")                \
    TOKEN(TOKEN_EXPORT, "export")            \
    TOKEN(TOKEN_STRUCT, "struct")            \
    TOKEN(TOKEN_ENUM, "enum")                \
    TOKEN(TOKEN_TRUE, "true")                \
    TOKEN(TOKEN_FALSE, "false")              \
    TOKEN(TOKEN_NONE, "none")                \
    TOKEN(TOKEN_OR, "or")                    \
    TOKEN(TOKEN_AND, "and")                  \
    TOKEN(TOKEN_NEW, "new")                  \
    TOKEN(TOKEN_CONTINUE, "continue")        \
    TOKEN(TOKEN_BREAK, "break")              \
    TOKEN(TOKEN_AT_SIGN, "@")                \
    TOKEN(TOKEN_NOT, "!")                    \
    TOKEN(TOKEN_DIF, "!=")                   \
    TOKEN(TOKEN_ARROW, "->")                 \
    TOKEN(TOKEN_COLON, ":")                  \
    TOKEN(TOKEN_DOUBLE_COLON, "::")          \
    TOKEN(TOKEN_DOT, ".")                    \
    TOKEN(TOKEN_SEMI, ";")                   \
    TOKEN(TOKEN_MARK, "?")                   \
    TOKEN(TOKEN_COMA, ",")                   \
    TOKEN(TOKEN_LPARENT, "(")                \
    TOKEN(TOKEN_RPARENT, ")")                \
    TOKEN(TOKEN_LBRACKET, "{")               \
    TOKEN(TOKEN_RBRACKET, "}")               \
    TOKEN(TOKEN_LBRACE, "[")                 \
    TOKEN(TOKEN_RBRACE, "]")                 \
    TOKEN(TOKEN_EQUAL, "=")                  \
    TOKEN(TOKEN_EQUAL_EQUAL, "==")           \
    TOKEN(TOKEN_PLUS, "+")                   \
    TOKEN(TOKEN_PLUS_EQUAL, "+=")            \
    TOKEN(TOKEN_MINUS, "-")                  \
    TOKEN(TOKEN_MINUS_EQUAL, "-=")           \
    TOKEN(TOKEN_MULT, "*")                   \
    TOKEN(TOKEN_MULT_EQUAL, "*=")            \
    TOKEN(TOKEN_DIV, "/")                    \
    TOKEN(TOKEN_DIV_EQUAL, "/=")             \
    TOKEN(TOKEN_MODULO, "%")                 \
    TOKEN(TOKEN_MODULO_EQUAL, "%=")          \
    TOKEN(TOKEN_SUP, ">")                    \
    TOKEN(TOKEN_SUP_EQUAL, ">=")             \
    TOKEN(TOKEN_INF, "<")                    \
    TOKEN(TOKEN_INF_EQUAL, "<=")             \
    TOKEN(TOKEN_EOF, "fin de fichier")

#define GENERATE_TOKEN_ENUM(TOKEN, TEXT) TOKEN,

typedef enum{
    FOREACH_TOKEN(GENERATE_TOKEN_ENUM)
}TokenEnum;

typedef struct{
    TokenEnum type;
    char *value;           // texte du jeton (alloué seulement si token_owns_value)
    int lenght;            // taille de value (une chaîne peut contenir \0)
    int line;
    int colone;
    bool newline_before;   // le jeton commence une nouvelle ligne
}token_t;

const char *token_to_string(TokenEnum type);
bool token_owns_value(TokenEnum type);
void freeToken(token_t *token);

#endif
