#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "include/scope.h"
#include "include/common.h"
#include "include/Value.h"


#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
#else
    #include <signal.h>
    #include <termios.h>
    #include <unistd.h>
    #include <sys/select.h>
#endif


typedef struct{
    const char *name;
    ceflo_func_t func;
}os_function;


#ifndef _WIN32

static struct termios saved_term;
static volatile sig_atomic_t term_is_raw = 0;

static void console_raw_off(void){
    if(term_is_raw){
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_term);
        term_is_raw = 0;
    }
}

// Ctrl+C pendant getch ne doit pas laisser le terminal sans écho.
static void console_on_signal(int sig){
    console_raw_off();
    signal(sig, SIG_DFL);
    raise(sig);
}

// Faux quand l'entrée n'est pas un terminal.
static bool console_raw_on(void){
    static bool installed = false;
    struct termios new_term;

    if (tcgetattr(STDIN_FILENO, &saved_term) != 0)
        return false;

    if(!installed){
        atexit(console_raw_off);
        signal(SIGINT, console_on_signal);
        signal(SIGTERM, console_on_signal);
        installed = true;
    }

    new_term = saved_term;
    new_term.c_lflag &= ~(ICANON | ECHO);
    new_term.c_cc[VMIN] = 1;
    new_term.c_cc[VTIME] = 0;

    term_is_raw = 1;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) != 0){
        term_is_raw = 0;
        return false;
    }
    return true;
}

// read et non getchar : le tampon de getchar cacherait les octets suivants à kbhit.
static value_t *console_read_key(void){
    if(!console_raw_on())
        return makeNone();

    unsigned char c;
    ssize_t count = read(STDIN_FILENO, &c, 1);

    console_raw_off();
    return count == 1 ? makeNumber(c) : makeNone();
}

#endif


static value_t * console_clrscr(struct VISITOR_STRUCT * visitor , ast_t * node , value_t **args , int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"clrscr" , argc, 0);
    printf("\033[2J\033[H");
    fflush(stdout);
    return makeNone();
}

static value_t * console_getchar(struct VISITOR_STRUCT * visitor , ast_t * node , value_t **args , int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"getchar" , argc, 0);
    fflush(stdout);
    #ifdef _WIN32
        return  makeNumber(_getch());
    #else
        return console_read_key();
    #endif
}


static value_t * console_kbhit(struct VISITOR_STRUCT * visitor , ast_t * node , value_t **args , int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"kbhit" , argc, 0);
    #ifdef _WIN32

        return makeBool(_kbhit() > 0);
    #else
        if(!console_raw_on())
            return makeBool(0);

        fd_set set;
        struct timeval timeout = {0, 0};

        FD_ZERO(&set);
        FD_SET(STDIN_FILENO, &set);

        int result = select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout);

        console_raw_off();

        return makeBool(result > 0);
    #endif
}

static value_t * console_getch(struct VISITOR_STRUCT * visitor , ast_t * node , value_t **args , int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"getch" , argc, 0);
    fflush(stdout);
    #ifdef _WIN32
        return makeNumber(_getch());
    #else
        return console_read_key();
    #endif
}

// La durée est en millisecondes.
static value_t * console_sleep(struct VISITOR_STRUCT * visitor , ast_t * node , value_t **args , int argc){
    (void)visitor;
    Value_check_args(node,"sleep" , argc, 1);
    Value_check_type(node,"sleep",args[0],NUMBER);
    fflush(stdout);
    #ifdef _WIN32
        Sleep((DWORD)AS_NUMBER(args[0]));
    #else
        usleep((useconds_t)(AS_NUMBER(args[0]) * 1000));
    #endif
    return makeNone();
}


static const os_function Console_List[] = {
    {"clrscr",&console_clrscr},
    {"getch",&console_getch},
    {"getchar",&console_getchar},
    {"kbhit",&console_kbhit},
    {"sleep",&console_sleep},

    {NULL,NULL}
};

void console_define_all(scope_t *scope){
    for(const os_function *item = Console_List; item->name != NULL; item++)
        scope_define(scope,item->name,makeBuiltin(item->name,item->func));
}
