#include <stdio.h>
#include "Value.h"
#include "scope.h"

int main(void){
    scope_t *fichier = newScope(NULL);
    scope_t *fonction = newScope(fichier);
    scope_define(fichier, "x", makeString("globale"));
    scope_define(fonction, "y", makeNumber(2));

    printf("scope_get(fonction, \"x\")    : %s\n",
        scope_get(fonction, "x") == NULL ? "NULL" : "trouvé");
    hash_t *x = scope_lookup(fonction, "x");
    printf("scope_lookup(fonction, \"x\") : %s\n", AS_STRING((value_t*)x->value)->str);
    printf("scope_lookup(fichier, \"y\")  : %s\n",
        scope_lookup(fichier, "y") == NULL ? "NULL" : "trouvé");

    // 40 noms dans une seule portée : combien par case ?
    static char noms[40][8];
    for(int i = 0; i < 40; i++){
        snprintf(noms[i], sizeof(noms[i]), "v%d", i);
        scope_define(fichier, noms[i], makeNumber(i));
    }
    for(int i = 0; i < TAB_SIZE; i++){
        int n = 0;
        for(hash_t *item = fichier->variables.TabHash[i]; item != NULL; item = item->next)
            n++;
        printf("case %d : %d noms\n", i, n);
    }
    return 0;
}
