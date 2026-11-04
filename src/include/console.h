#ifndef __CONSOLE_H__
#define __CONSOLE_H__

    #include "scope.h"

    // Le module `console` : les fichiers, les dossiers et le système.
    // Il est compilé dans l'interpréteur : `from console import [lire]` marche depuis n'importe où.
    void console_define_all(scope_t *scope);


#endif