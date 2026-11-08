/**
 * @file main.c
 * @brief la ligne de commande : `ceflo [--dis] fichier.cf [arguments du script]`
 *
 *      [Lexer] ==> [Parser] ==> [AST] ==> [Compilateur] ==> [Bytecode] ==> [VM]
 *
 *      Avec --dis, le bytecode de chaque fonction s'affiche avant l'exécution.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __GLIBC__
    #include <malloc.h>
#endif

#include "vm.h"
#include "error.h"

// La VM est grande (ses CallFrame) : elle ne tient pas sur la pile C.
static VM vm;

// Appelée à chaque sortie du programme, même après une erreur ou un exit() du script.
static void ceflo_clean(void){
    fflush(stdout);
    vm_free(&vm);
    clearAst();
    error_free_sources();
}

int main(int argc, char **argv){
    int first = 1;
    bool disassemble = argc > 1 && strcmp(argv[1],"--dis") == 0;
    if(disassemble)
        first = 2;

    if(argc <= first){
        fprintf(stderr,"CeFlo\n");
        fprintf(stderr,"utilisation : %s fichier.cf [arguments du script]\n",argv[0]);
        return 2;
    }

    #ifdef __GLIBC__
        // `texte += ligne` alloue et libère sans arrêt de longues chaînes. Par défaut,
        // glibc les rend au système à chaque fois : on les garde pour les réutiliser.
        mallopt(M_MMAP_THRESHOLD,64 * 1024 * 1024);
        mallopt(M_TRIM_THRESHOLD,128 * 1024 * 1024);
    #endif

    ceflo_stack_init();
    vm_init(&vm,argc - first,argv + first);
    vm.disassemble = disassemble;
    atexit(ceflo_clean);

    return vm_run_file(&vm,argv[first]);
}
