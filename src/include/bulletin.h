#ifndef __BULLETIN_H__
#define __BULLETIN_H__

    #include "visitor.h"

    typedef struct BULLETIN_STRUCT {
        char *name;
        ast_t* (*func)(visitor_t * visitor , ast_t * node , list_ast_t *args );
    }bulletin;



    ast_t *bulletin_switch_function(visitor_t * visitor , ast_t *node , list_ast_t *args);
    ast_t * bulletin_println(visitor_t * visitor , ast_t * node , list_ast_t *args );
    ast_t * bulletin_printaddr(visitor_t * visitor , ast_t * node , list_ast_t *args );


    #define BULLETIN_LIST_COUNT 3

    static bulletin Bulletin_List[] = {
        {"println",&bulletin_println},
        {"printaddr",&bulletin_printaddr},

        {NULL,NULL}
    };
    



#endif