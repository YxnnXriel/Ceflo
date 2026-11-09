// La taille qu'aurait un noeud si chaque sorte avait ses propres champs, dans une union.
#include <stdio.h>
#include <stdbool.h>

#include "token.h"
#include "list.h"

typedef struct NODE{
    int type;
    const char *file;
    int line;
    int colone;
    struct NODE *next;
    union{
        double number;
        struct{ char *text; int lenght; } string;
        struct{ char *name; } variable;
        struct{ struct NODE *left, *right; TokenEnum op; } binop;
        struct{ struct NODE *condition, *body, *other, *otherwise; } branch;
        struct{ struct NODE *init, *condition, *step, *body; } loop;
        struct{ char *name, *return_type; list_t *args; struct NODE *body; bool self; } function;
        struct{ char *name, *type; struct NODE *value, *next; bool is_const; } definition;
        struct{ list_t *items; bool has_definition; } block;
    } as;
}node_t;

int main(void){
    printf("avec une union : %zu octets\n",sizeof(node_t));
    return 0;
}
