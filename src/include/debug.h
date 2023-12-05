#ifndef __DEBUG_H__
#define __DEBUG_H__

    #include "visitor.h"

    void Debug_ast_is_null(ast_t *node);
    void Debug_Print(const char *func_name);
    uint8_t Debug_Print_Ast_Type(ast_t *node);
    
    void Debug_value_is_null(void *x);



#endif