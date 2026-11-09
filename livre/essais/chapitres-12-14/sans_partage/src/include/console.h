#ifndef __CONSOLE_H__
#define __CONSOLE_H__

    #include "scope.h"

    // Le module `console` : l'écran, le clavier et les pauses.
    // Il est compilé dans l'interpréteur : `from console import [getch]` marche depuis n'importe où.
    void console_define_all(scope_t *scope);


#endif