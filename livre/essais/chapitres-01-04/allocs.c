// Compte les appels à ceflo_alloc qui demandent la taille d'un noeud.
// À lier avec -Wl,--wrap=ceflo_alloc : chaque appel venu d'un autre fichier passe par ici.
#include <stdio.h>
#include <stdlib.h>

#include "ast.h"

void *__real_ceflo_alloc(size_t size);

static int allocated;

void *__wrap_ceflo_alloc(size_t size){
    if(size == sizeof(ast_t))
        allocated += 1;
    return __real_ceflo_alloc(size);
}

static void print_allocated(void){
    printf("noeuds alloués : %d\n",allocated);
}

__attribute__((constructor)) static void start(void){
    atexit(print_allocated);
}
