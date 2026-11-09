/**
 * @file ast.c
 * @author Yann Ariel (yannariel13@gmail.com)
 * @copyright Copyright (c) 2023
 */

#include <stdlib.h>
#include <string.h>

#include "include/ast.h"

static ast_t *first_ast;

ast_t *newAst(ast_type type){
    ast_t *node = ceflo_alloc(sizeof(*node));
    node->type = type;
    node->next = first_ast;
    first_ast = node;
    return node;
}

ast_t *newAstWithLine(ast_type type, const char *file, int line, int colone){
    ast_t *node = newAst(type);
    node->file = file;
    node->line = line;
    node->colone = colone;
    return node;
}

static void freeAst(ast_t *node){
    free(node->variable_definition_name);
    free(node->variable_definition_static_type_name);
    free(node->variable_name);
    free(node->function_definition_name);
    free(node->function_definition_return_type_name);
    free(node->function_call_name);
    free(node->string_value);
    free(node->struct_definition_name);
    List_Free(node->function_definition_args);
    List_Free(node->function_call_args);
    List_Free(node->struct_definition_item);
    List_Free(node->list_compound);
    free(node);
}

void clearAst(void){
    while(first_ast != NULL){
        ast_t *next = first_ast->next;
        freeAst(first_ast);
        first_ast = next;
    }
}

ast_t *GetFunctionInStruct(ast_t *structure, const char *name){
    for(int i = 0; i < structure->struct_definition_item->Size; i++){
        ast_t *item = List_Get(structure->struct_definition_item,i);
        if(item->type == AST_FUNCTION_DEFINITION && strcmp(name,item->function_definition_name) == 0)
            return item;
    }
    return NULL;
}
