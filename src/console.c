#include <stdio.h>
#include <stdlib.h>

#include "include/scope.h"
#include "include/common.h"
#include "include/Value.h"


#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
#else
    #include <termios.h>
    #include <unistd.h>
    #include <sys/select.h>
#endif


typedef struct{
    const char *name;
    ceflo_func_t func;
}os_function;


static value_t * console_clrscr(struct VISITOR_STRUCT * visitor , ast_t * node , value_t **args , int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"clrsrc" , argc, 0);
    printf("\0033[2J\033[H");
    fflush(stdout);
    return makeNone();
}

static value_t * console_getchar(struct VISITOR_STRUCT * visitor , ast_t * node , value_t **args , int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"getchar" , argc, 0);
    #ifdef _WIN32

        return  makeNumber(_getch());

    #else
        struct termios old_term, new_term;
        if (tcgetattr(STDIN_FILENO, &old_term) != 0)
            return makeNone();

        new_term = old_term;
        new_term.c_lflag &= ~(ICANON | ECHO);

        if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) != 0)
            return makeNone();

        int c = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
        return makeNumber(c);
    #endif
}


static value_t * console_kbhit(struct VISITOR_STRUCT * visitor , ast_t * node , value_t **args , int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"kbhit" , argc, 0);
    #ifdef _WIN32

        return makeBool(_kbhit() > 0);
    #else
        struct termios old_term, new_term;

        if (tcgetattr(STDIN_FILENO, &old_term) != 0)
            return 0;

        new_term = old_term;
        new_term.c_lflag &= ~(ICANON | ECHO);

        if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) != 0)
            return makeBool(0);

        fd_set set;
        struct timeval timeout = {0, 0};

        FD_ZERO(&set);
        FD_SET(STDIN_FILENO, &set);

        int result = select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout);

        tcsetattr(STDIN_FILENO, TCSANOW, &old_term);

        return makeBool(result > 0);
    #endif
}

static value_t * console_getch(struct VISITOR_STRUCT * visitor , ast_t * node , value_t **args , int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"getch" , argc, 0);
    #ifdef _WIN32
        return makeNumber(_getch());
    #else


        struct termios old_term, new_term;

        if (tcgetattr(STDIN_FILENO, &old_term) != 0)
            return makeNone();

        new_term = old_term;
        new_term.c_lflag &= ~(ICANON | ECHO);
        new_term.c_cc[VMIN] = 1;
        new_term.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) != 0)
            return makeNone();

        int c = getchar();

        tcsetattr(STDIN_FILENO, TCSANOW, &old_term);

        return makeNumber(c);

    #endif
}


static const os_function Console_List[] = {
    {"clrscr",&console_clrscr},
    {"getch",&console_getch},
    {"getchar",&console_getchar},
    {"kbhit",&console_kbhit},

    {NULL,NULL}
};

void console_define_all(scope_t *scope){
    for(const os_function *item = Console_List; item->name != NULL; item++)
        scope_define(scope,item->name,makeBuiltin(item->name,item->func));
}
