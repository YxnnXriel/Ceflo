#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "include/clear.h"
#include "include/bulletin.h"





ast_t * bulletin_println(visitor_t * visitor , ast_t * node , list_ast_t *args ){

    int i = 0;
    for ( i = 0; i < (int) args->size; i++){
        ast_t * child = List_Ast_Get(args,i);
        child = visitor_visit(visitor,child);
        printValueAst(child);
    }
    
    printf("\n");

    ast_t * count = newAst(AST_NUMBER);
    count->value.as.numberValue = i;
    count->value.type = NUMBER;

    return count; 
}

ast_t * bulletin_print(visitor_t * visitor , ast_t * node , list_ast_t *args ){

    int i = 0;
    for ( i = 0; i < (int) args->size; i++){
        ast_t * child = List_Ast_Get(args,i);
        child = visitor_visit(visitor,child);
        printValueAst(child);
    }
    
    ast_t * count = newAst(AST_NUMBER);
    count->value.as.numberValue = i;
    count->value.type = NUMBER;

    return count; 
}



ast_t *bulletin_format(visitor_t *visitor, ast_t *node, list_ast_t *args) {
    // Vérifier que la liste d'arguments contient au moins deux éléments (chaîne de formatage et au moins une valeur à insérer)
    if (args->size < 1) {
        fprintf(stderr, "Erreur : nombre d'arguments insuffisant pour la fonction bulletin_format.\n");
        clear_ceflo_clean();
        exit(EXIT_FAILURE);
    }

    // Récupérer la chaîne de formatage (premier argument)
    ast_t *format_string_ast = List_Ast_Get(args, 0);
    if (format_string_ast->type != AST_STRING) {
        fprintf(stderr, "Erreur : le premier argument de bulletin_format doit être une chaîne de caractères.\n");
        clear_ceflo_clean();
        exit(EXIT_FAILURE);
    }
    char *format_string = format_string_ast->value.as.stringValue->str;
    int i = 1;

    // Parcourir la chaîne de formatage et la traiter
    for (int j = 0; format_string[j] != '\0'; j++) {
        if (format_string[j] == '{') {
            // Rechercher l'indice de fin du placeholder
            int end_index = j + 1;
            int nbr = 0;
            while (format_string[end_index] != '}' && format_string[end_index] != '%' && format_string[end_index] != '\0') {
                if(isdigit(format_string[end_index])){
                    nbr = nbr * 10  + (format_string[end_index] - '0');
                }
                end_index++;
            }
            if(format_string[end_index] == '%'){
                printf("{");
                j++;
            }
            else if (format_string[end_index] == '}') {
                int arg_index = (nbr == 0) ? i : nbr;

                if (arg_index >= 1 && arg_index < (int)args->size) {
                    // Si l'indice est valide, récupérer la valeur correspondante et l'afficher
                    ast_t *value_ast = List_Ast_Get(args, arg_index);
                    ast_t *formatted_value_ast = visitor_visit(visitor, value_ast);
                    printValueAst(formatted_value_ast);
                    j = end_index; // Passer à la fin du placeholder }
                    i = nbr+1;
                } else {
                    // Si l'indice est invalide, afficher un message d'erreur
                    fprintf(stderr, "Erreur : indice d'argument invalide dans la chaîne de formatage.\n");
                    clear_ceflo_clean();
                    exit(EXIT_FAILURE);
                }

                if (nbr == 0) {
                    // Si le placeholder est vide, passer à l'argument suivant
                    i++;
                }
            } else {
                // Si le placeholder n'est pas fermé, afficher un message d'erreur
                fprintf(stderr, "Erreur : placeholder non fermé dans la chaîne de formatage.\n");
                clear_ceflo_clean();
                exit(EXIT_FAILURE);
            }
        } else {
            // Si ce n'est pas un placeholder, afficher le caractère tel quel
            printf("%c", format_string[j]);
        }
    }

    printf("\n");

    // Retourner le nombre d'arguments utilisés pour le formatage
    ast_t *count = newAst(AST_NUMBER);
    count->value.as.numberValue = args->size - 1; // Soustraire 1 car le premier argument est la chaîne de formatage
    count->value.type = NUMBER;
    return count;
}

ast_t * bulletin_input(visitor_t * visitor , ast_t * node , list_ast_t *args ){
    bulletin_print(visitor, node, args);

    char buffer[BUFSIZ];
    fgets(buffer, BUFSIZ, stdin);

    // Supprimer le caractère de nouvelle ligne de la chaîne saisie
    buffer[strlen(buffer) - 1] = '\0';

    ast_t *string = newAst(AST_STRING);
    string->value.as.stringValue = new_String(buffer);

    return string;
}


ast_t * bulletin_printaddr(visitor_t * visitor , ast_t * node , list_ast_t *args ){

    for (int i = 0; i < (int) args->size; i++){
        ast_t * child = List_Ast_Get(args,i);
        child = visitor_visit(visitor,child);
        printf("%p",(void*)child);
    }

    printf("\n");

    return node;

}

ast_t *bulletin_switch_function(visitor_t * visitor , ast_t *node , list_ast_t *args){
    if(node->function_call_name == NULL)
        return NULL;
    const char * function_name = node->function_call_name;

    for (int i = 0; i < BULLETIN_LIST_COUNT -1; i++)
        if(strcmp(function_name,Bulletin_List[i].name) == 0)
            return Bulletin_List[i].func(visitor,node,&node->function_call_args);

        
    return NULL;
}
