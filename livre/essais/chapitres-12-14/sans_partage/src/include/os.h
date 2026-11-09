#ifndef __OS_H__
#define __OS_H__

    #include "visitor.h"

    // Le module `os` : les fichiers, les dossiers et le système.
    // Il est compilé dans l'interpréteur : `from os import [lire]` marche depuis n'importe où.
    void os_define_all(scope_t *scope);

#endif
