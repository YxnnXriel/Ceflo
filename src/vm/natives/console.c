#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "ceflo.h"


#ifdef _WIN32
    #include <conio.h>
    #include <windows.h>
#else
    #include <signal.h>
    #include <termios.h>
    #include <unistd.h>
    #include <sys/select.h>
#endif


// Même forme que la table de os.c, qui la lit avec natives_standard_module.
typedef struct NATIVE_FUNCTION{
    const char *name;
    NativeFn function;
}native_function;


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
static Value console_read_key(void){
    if(!console_raw_on())
        return NONE_VAL;

    unsigned char c;
    ssize_t count = read(STDIN_FILENO, &c, 1);

    console_raw_off();
    return count == 1 ? NUMBER_VAL(c) : NONE_VAL;
}

#endif


static Value console_clrscr(VM *vm, int argc, Value *args){
    (void)args;
    native_check_args(vm,"clrscr",argc,0);
    printf("\033[2J\033[H");
    fflush(stdout);
    return NONE_VAL;
}

static Value console_getchar(VM *vm, int argc, Value *args){
    (void)args;
    native_check_args(vm,"getchar",argc,0);
    fflush(stdout);
    #ifdef _WIN32
        return NUMBER_VAL(_getch());
    #else
        return console_read_key();
    #endif
}


static Value console_kbhit(VM *vm, int argc, Value *args){
    (void)args;
    native_check_args(vm,"kbhit",argc,0);
    #ifdef _WIN32

        return BOOL_VAL(_kbhit() > 0);
    #else
        if(!console_raw_on())
            return BOOL_VAL(false);

        fd_set set;
        struct timeval timeout = {0, 0};

        FD_ZERO(&set);
        FD_SET(STDIN_FILENO, &set);

        int result = select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout);

        console_raw_off();

        return BOOL_VAL(result > 0);
    #endif
}

static Value console_getch(VM *vm, int argc, Value *args){
    (void)args;
    native_check_args(vm,"getch",argc,0);
    fflush(stdout);
    #ifdef _WIN32
        return NUMBER_VAL(_getch());
    #else
        return console_read_key();
    #endif
}

// La durée est en millisecondes.
static Value console_sleep(VM *vm, int argc, Value *args){
    native_check_args(vm,"sleep",argc,1);
    native_check_type(vm,"sleep",args[0],"number");
    fflush(stdout);
    #ifdef _WIN32
        Sleep((DWORD)AS_NUMBER(args[0]));
    #else
        usleep((useconds_t)(AS_NUMBER(args[0]) * 1000));
    #endif
    return NONE_VAL;
}


const native_function Console_List[] = {
    {"clrscr",&console_clrscr},
    {"getch",&console_getch},
    {"getchar",&console_getchar},
    {"kbhit",&console_kbhit},
    {"sleep",&console_sleep},
    {NULL,NULL}
};
