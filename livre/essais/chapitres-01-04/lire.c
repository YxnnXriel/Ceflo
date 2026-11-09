// Lit un fichier CeFlo, l'affiche avec ses numéros de ligne,
// puis, si on lui donne une ligne et une colonne, montre une erreur à cet endroit.
#include <stdio.h>
#include <stdlib.h>

#include "str.h"
#include "error.h"

int main(int argc, char **argv){
    if(argc != 2 && argc != 4){
        fprintf(stderr,"utilisation : %s fichier.cf [ligne colonne]\n",argv[0]);
        return 2;
    }

    char *code = readFile(argv[1]);
    if(code == NULL){
        fprintf(stderr,"impossible d'ouvrir `%s`\n",argv[1]);
        return 1;
    }

    // À partir d'ici, le texte appartient au module des erreurs.
    const char *name = error_add_source(argv[1],code);

    int line = 1;
    printf("%5d | ",line);
    for(char *c = code; *c != '\0'; c++){
        putchar(*c);
        if(*c == '\n' && c[1] != '\0')
            printf("%5d | ",++line);
    }

    if(argc == 4)
        error_syntax(name,atoi(argv[2]),atoi(argv[3]),"on regarde ici");

    error_free_sources();
    return 0;
}
