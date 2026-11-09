// Compte les noeuds de l'arbre d'un fichier, et ce qu'ils coûtent en mémoire.
#include <stdio.h>

#include "parser.h"
#include "error.h"

static int count_node(ast_t *node);

static int count_list(list_t *list){
    int total = 0;
    for(int i = 0; list != NULL && i < list->Size; i++)
        total += count_node(List_Get(list,i));
    return total;
}

// Tous les noeuds ont les mêmes champs : on suit chaque pointeur, sans regarder la sorte.
static int count_node(ast_t *node){
    if(node == NULL)
        return 0;

    ast_t *children[] = {
        node->variable_definition_value, node->function_definition_body,
        node->left, node->right, node->return_expr,
        node->if_expr, node->if_body, node->if_other, node->else_body,
        node->while_expr, node->while_body,
        node->for_init, node->for_condition, node->for_solve, node->for_body,
        node->assignement_left, node->assignement_value, node->list_ptr,
    };

    int total = 1;
    for(size_t i = 0; i < sizeof(children) / sizeof(children[0]); i++)
        total += count_node(children[i]);
    total += count_list(node->function_definition_args);
    total += count_list(node->function_call_args);
    total += count_list(node->struct_definition_item);
    total += count_list(node->list_compound);
    return total;
}

int main(int argc, char **argv){
    ceflo_stack_init();
    printf("un noeud : %zu octets\n",sizeof(ast_t));

    for(int i = 1; i < argc; i++){
        lexer_t lexer;
        if(!newLexer(&lexer,argv[i])){
            fprintf(stderr,"impossible d'ouvrir `%s`\n",argv[i]);
            return 1;
        }
        size_t text = lexer.lenght_file_code;
        int nodes = count_node(parser_parse(&lexer));
        printf("%-28s %6zu octets de texte, %5d noeuds, %8zu octets d'arbre\n",
            argv[i],text,nodes,(size_t)nodes * sizeof(ast_t));
    }

    clearAst();
    error_free_sources();
    return 0;
}
