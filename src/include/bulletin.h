#ifndef __BULLETIN_H__
#define __BULLETIN_H__

    #include "visitor.h"
    #include "typer.h"

    typedef struct BULLETIN_STRUCT {
        char *name;
        ast_t* (*func)(visitor_t * visitor , ast_t * node , list_ast_t *args );
    }bulletin;



    ast_t *bulletin_switch_function(visitor_t * visitor , ast_t *node , list_ast_t *args);
    ast_t * bulletin_println(visitor_t * visitor , ast_t * node , list_ast_t *args );
    ast_t * bulletin_print(visitor_t * visitor , ast_t * node , list_ast_t *args );
    ast_t * bulletin_printaddr(visitor_t * visitor , ast_t * node , list_ast_t *args );
    ast_t * bulletin_input(visitor_t * visitor , ast_t * node , list_ast_t *args );
    ast_t * bulletin_printf(visitor_t * visitor, ast_t * node, list_ast_t * args);
    ast_t *bulletin_format(visitor_t *visitor, ast_t *node, list_ast_t *args);
    ast_t *bulletin_nextInt(visitor_t *visitor, ast_t *node, list_ast_t *args);



    #define BULLETIN_LIST_COUNT 8

    static bulletin Bulletin_List[] = {
        {"println",&bulletin_println},
        {"print",&bulletin_print},
        {"printaddr",&bulletin_printaddr},
        {"input",&bulletin_input},
        {"format",&bulletin_format},
        {"printf",&bulletin_printf},
        {"nextInt",&bulletin_nextInt},
        {NULL,NULL}
    };
    



#endif