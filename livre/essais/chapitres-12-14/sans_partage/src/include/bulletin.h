#ifndef __BULLETIN_H__
#define __BULLETIN_H__

    #include "visitor.h"

    // Ajoute println, input, format... et la liste `argv` dans une portée.
    void bulletin_define_all(scope_t *scope, int argc, char **argv);
    void bulletin_free(void);

#endif
