// Affiche l'arbre qu'un fichier CeFlo devient une fois passé dans le parser.
#include <stdio.h>

#include "parser.h"
#include "error.h"

static const char *NAMES[] = {
    "NOOP", "COMPOUND", "NONE", "BOOL", "CHAR", "NUMBER", "STRING", "LIST", "MAP",
    "LFUNC", "VARIABLE", "OBJ_ACCESS", "LIST_ACCESS", "FUNCTION_CALL", "STRUCT",
    "TYPE_CALL", "BINOP", "UNOP", "TERNARY", "VARIABLE_DEFINITION",
    "FUNCTION_DEFINITION", "STRUCT_DEFINITION", "ENUM_DEFINITION", "ASSIGNEMENT",
    "RETURN", "BREAK", "CONTINUE", "IF", "MATCH", "CASE", "WHILE", "FOR", "FOR_IN",
    "INCLUDE", "EXPORT",
};

static void print_node(const char *role, ast_t *node, int depth);

static void print_list(const char *role, list_t *list, int depth){
    if(list == NULL)
        return;
    for(int i = 0; i < list->Size; i++)
        print_node(role,List_Get(list,i),depth);
}

// Une ligne par noeud : son rôle chez son parent, sa sorte, et ce qu'il contient en propre.
static void print_line(const char *role, ast_t *node, int depth){
    printf("%3d:%-3d %*s",node->line,node->colone,depth * 2,"");
    if(role != NULL)
        printf("%s: ",role);
    printf("%s",NAMES[node->type]);

    switch(node->type){
        case AST_NUMBER: printf(" %g",node->number_value); break;
        case AST_STRING: printf(" \"%s\"",node->string_value); break;
        case AST_CHAR: printf(" '%c'",node->char_value); break;
        case AST_BOOL: printf(" %s",node->bool_value ? "true" : "false"); break;
        case AST_VARIABLE:
        case AST_OBJ_ACCESS:
        case AST_INCLUDE: printf(" %s",node->variable_name); break;
        case AST_BINOP:
        case AST_UNOP:
        case AST_ASSIGNEMENT: printf(" %s",token_to_string(node->op)); break;
        case AST_VARIABLE_DEFINITION:
            printf(" %s%s",node->is_const ? "const " : "",node->variable_definition_name);
            break;
        case AST_FUNCTION_DEFINITION:
        case AST_LFUNC: printf(" %s",node->function_definition_name); break;
        case AST_STRUCT:
        case AST_TYPE_CALL: printf(" %s",node->function_call_name); break;
        case AST_STRUCT_DEFINITION:
        case AST_ENUM_DEFINITION: printf(" %s",node->struct_definition_name); break;
        default: break;
    }
    if(node->variable_definition_static_type_name != NULL)
        printf(" ::%s",node->variable_definition_static_type_name);
    if(node->function_definition_return_type_name != NULL)
        printf(" : %s",node->function_definition_return_type_name);
    putchar('\n');
}

static void print_node(const char *role, ast_t *node, int depth){
    if(node == NULL)
        return;
    print_line(role,node,depth);
    depth += 1;

    switch(node->type){
        case AST_VARIABLE_DEFINITION:
            print_node("valeur",node->variable_definition_value,depth);
            // var a, b : les suivantes sont accrochées à `left`, au même niveau.
            print_node(NULL,node->left,depth - 1);
            return;
        case AST_FUNCTION_DEFINITION:
        case AST_LFUNC:
            print_list("param",node->function_definition_args,depth);
            print_node("corps",node->function_definition_body,depth);
            return;
        case AST_STRUCT_DEFINITION:
        case AST_ENUM_DEFINITION:
            print_list(NULL,node->struct_definition_item,depth);
            return;
        case AST_FUNCTION_CALL:
            print_node("appelé",node->left,depth);
            print_list("arg",node->function_call_args,depth);
            return;
        case AST_STRUCT:
        case AST_TYPE_CALL:
            print_list("arg",node->function_call_args,depth);
            return;
        case AST_OBJ_ACCESS:
        case AST_UNOP:
        case AST_BINOP:
            print_node("gauche",node->left,depth);
            print_node("droite",node->right,depth);
            return;
        case AST_LIST_ACCESS:
            print_node("liste",node->left,depth);
            print_node("index",node->list_ptr,depth);
            return;
        case AST_ASSIGNEMENT:
            print_node("cible",node->assignement_left,depth);
            print_node("valeur",node->assignement_value,depth);
            return;
        case AST_RETURN:
            print_node(NULL,node->return_expr,depth);
            return;
        case AST_IF:
        case AST_TERNARY:
            print_node("si",node->if_expr,depth);
            print_node("alors",node->type == AST_IF ? node->if_body : node->if_other,depth);
            if(node->type == AST_IF)
                print_node("sinon si",node->if_other,depth);
            print_node("sinon",node->else_body,depth);
            return;
        case AST_MATCH:
            print_node("sujet",node->if_expr,depth);
            print_list(NULL,node->list_compound,depth);
            print_node("sinon",node->else_body,depth);
            return;
        case AST_CASE:
            print_list("valeur",node->list_compound,depth);
            print_node("bloc",node->if_body,depth);
            return;
        case AST_WHILE:
            print_node("tant que",node->while_expr,depth);
            print_node("corps",node->while_body,depth);
            return;
        case AST_FOR:
        case AST_FOR_IN:
            print_node("départ",node->for_init,depth);
            print_node(node->type == AST_FOR ? "condition" : "dans",node->for_condition,depth);
            print_node("pas",node->for_solve,depth);
            print_node("corps",node->for_body,depth);
            return;
        default:
            print_list(NULL,node->list_compound,depth);
            return;
    }
}

int main(int argc, char **argv){
    if(argc != 2){
        fprintf(stderr,"utilisation : %s fichier.cf\n",argv[0]);
        return 2;
    }

    // Le parser surveille la pile C : il doit savoir où elle commence.
    // ceflo_stack_init();

    lexer_t lexer;
    if(!newLexer(&lexer,argv[1])){
        fprintf(stderr,"impossible d'ouvrir `%s`\n",argv[1]);
        return 1;
    }

    ast_t *program = parser_parse(&lexer);
    print_node(NULL,program,0);

    clearAst();
    error_free_sources();
    return 0;
}
