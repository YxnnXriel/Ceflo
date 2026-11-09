/**
 * @file main.c
 * @author Yann Ariel (yannariel13@gmail.com)
 * @copyright Copyright (c) 2023
 *
 * L'interpréteur suit un parcours simple et basique :
 *      - le lexer découpe le fichier source en jetons
 *      - le parser donne un sens aux jetons et construit l'AST
 *      - le visitor parcourt l'arbre et exécute chaque noeud
 *
 *          [Lexer] ==> [Parser] ==> [AST] ==> [Visitor]
 *
 * Il m'a été demandé par une amie, à qui j'ai dédié son nom.
 */

#include <stdio.h>
#include <stdlib.h>

#include "include/visitor.h"
#include "include/bulletin.h"
#include "include/error.h"

// Appelée à chaque sortie du programme, même après une erreur.
static void ceflo_clean(void){
    fflush(stdout);
    visitor_free();
    bulletin_free();
    gc_free_all();
    Hash_Free_Pool();
    clearAst();
    error_free_sources();
}

int main(int argc, char **argv){
    if(argc < 2){
        fprintf(stderr,"CeFlo\n");
        fprintf(stderr,"utilisation : %s fichier.cf [arguments du script]\n",argv[0]);
        return 2;
    }

    ceflo_stack_init();
    atexit(ceflo_clean);

    visitor_t visitor;
    return visitor_run_file(&visitor,argc - 1,argv + 1);
}
