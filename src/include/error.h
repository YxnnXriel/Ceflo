#ifndef __ERROR_H__
#define __ERROR_H__

    #include <stdarg.h>
    #include "lexer.h"
#define FOREACH_ERROR(ERR)    \
        ERR(SUCCES)    \
        ERR(VARIABLE_DEFINITION_ERROR)    \
        ERR(FUNCTION_DEFINITION_ERROR)    \
        ERR(NONE_ERROR)    \
        ERR(BOOL_ERROR)    \
        ERR(CHAR_ERROR)    \
        ERR(INT_ERROR)    \
        ERR(FLOAT_ERROR)    \
        ERR(STRING_ERROR)    \
        ERR(FUNCTION_TYPE_ERROR)    \
        ERR(FUNCTION_CALL_ERROR)    \
        ERR(EXPRESSION_ERROR)    \
        ERR(TYPE_ERROR)    \
        ERR(RETURN_ERROR)   \
        ERR(SYNTAX_ERROR)
        

#define GENERATE_ERROR_ENUM(ENUM) ENUM,
#define GENERATE_STRING_ERROR(STRING) #STRING,

typedef enum{
    FOREACH_ERROR(GENERATE_ERROR_ENUM)
}errorE;

static const char *ERROR_TO_STRING[]={
    FOREACH_ERROR(GENERATE_STRING_ERROR)
};

void Print_error(lexer_t * lexer , char *fmt, ...);


void Print_visitor_error(int line, int colone,char *file_name,char *format,...);

#endif