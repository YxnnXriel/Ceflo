#ifndef __VALUE_H__
#define __VALUE_H__


    #include "visitor.h"


    // LIST
    ast_t * value_List_lenght(visitor_t * visitor , list_t *list , ast_t * value);
    ast_t * value_List_append(visitor_t * visitor , list_t *list , ast_t * value);
    ast_t * value_List_get(visitor_t * visitor , list_t *list , ast_t * value);
    ast_t * value_List_deleted(visitor_t * visitor , list_t *list , ast_t * value);
    ast_t * value_List_print(visitor_t * visitor , list_t *list , ast_t * value);

    struct value_list{
        char *name;
        ast_t* (*func_list_name)(visitor_t * visitor , list_t *list , ast_t * value);
    };
    
    ast_t * value_list_switch_function(visitor_t * visitor,list_t *list , ast_t *value);

    #define VALUE_LIST_ATTR 5
    static struct value_list attr_value_list[] = {
        {"append",&value_List_append},
        {"get",&value_List_get},
        {"delete",&value_List_deleted},
        {"lenght",&value_List_lenght},
        {"print",&value_List_print},
    };



    // NUMBER

    // CHAR

    // STRING

    // OBJECT


    // SOME VALUE





#endif