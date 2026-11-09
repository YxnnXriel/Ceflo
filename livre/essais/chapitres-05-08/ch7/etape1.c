// Étape 1 du chapitre 7 : des expressions, des variables et println.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "visitor.h"
#include "error.h"
#include "parser.h"
#include "value_expr.h"
#include "typer.h"
#include "bulletin.h"

static value_t *visitor_visit_statements(visitor_t *visitor, ast_t *node){
    for(int i = 0; i < node->list_compound->Size; i++){
        size_t mark = gc_temp_mark();
        visitor_visit(visitor,List_Get(node->list_compound,i));
        gc_temp_reset(mark);
    }
    return NULL;
}

static value_t *visitor_visit_variable_definition(visitor_t *visitor, ast_t *node){
    for(; node != NULL; node = node->left){
        value_t *value = node->variable_definition_value != NULL ?
            visitor_visit(visitor,node->variable_definition_value) : makeNone();
        scope_define(visitor->scope,node->variable_definition_name,value);
    }
    return NULL;
}

static value_t *visitor_visit_variable(visitor_t *visitor, ast_t *node){
    hash_t *item = scope_lookup(visitor->scope,node->variable_name);
    if(item == NULL)
        visitor_error(node,"`%s` n'est pas définie",node->variable_name);
    return item->value;
}

static value_t *visitor_visit_binop(visitor_t *visitor, ast_t *node){
    value_t *left = visitor_visit(visitor,node->left);
    value_t *right = visitor_visit(visitor,node->right);
    return value_expr(visitor,node,left,node->op,right);
}

value_t *visitor_call(visitor_t *visitor, ast_t *node, value_t *function, value_t **args, int argc){
    if(function->type == BUILTIN)
        return function->as.builtinValue.function(visitor,node,args,argc);
    visitor_error(node,"%s n'est pas une fonction",Value_type_name(function));
}

static value_t *visitor_visit_function_call(visitor_t *visitor, ast_t *node){
    value_t *function = visitor_visit(visitor,node->left);
    int argc = node->function_call_args->Size;
    value_t *args[argc > 0 ? argc : 1];
    for(int i = 0; i < argc; i++)
        args[i] = visitor_visit(visitor,List_Get(node->function_call_args,i));
    return visitor_call(visitor,node,function,args,argc);
}

static value_t *visitor_dispatch(visitor_t *visitor, ast_t *node){
    switch(node->type){
        case AST_NONE: return makeNone();
        case AST_BOOL: return makeBool(node->bool_value);
        case AST_CHAR: return makeChar(node->char_value);
        case AST_NUMBER: return makeNumber(node->number_value);
        case AST_STRING: return makeStringLenght(node->string_value,node->string_lenght);
        case AST_VARIABLE: return visitor_visit_variable(visitor,node);
        case AST_BINOP: return visitor_visit_binop(visitor,node);
        case AST_FUNCTION_CALL: return visitor_visit_function_call(visitor,node);
        case AST_VARIABLE_DEFINITION: return visitor_visit_variable_definition(visitor,node);
        default:
            visitor_error(node,"ce noeud n'est pas encore exécuté (type %d)",node->type);
    }
}

value_t *visitor_visit(visitor_t *visitor, ast_t *node){
    value_t *value = visitor_dispatch(visitor,node);
    if(value != NULL)
        gc_temp_push(value);
    return value;
}

int visitor_run_file(visitor_t *visitor, int argc, char **argv){
    memset(visitor,0,sizeof(*visitor));
    visitor->builtins = newScope(NULL);
    gc_root(visitor->builtins);
    bulletin_define_all(visitor->builtins,argc,argv);

    lexer_t lexer;
    if(!newLexer(&lexer,argv[0])){
        fprintf(stderr,"erreur: impossible d'ouvrir `%s`\n",argv[0]);
        exit(1);
    }
    ast_t *program = parser_parse(&lexer);

    visitor->scope = newScope(visitor->builtins);
    gc_root(visitor->scope);
    visitor_visit_statements(visitor,program);
    return 0;
}

// Ce qui arrivera aux étapes suivantes.
value_t *visitor_new_object(visitor_t *visitor, ast_t *node, value_t *structure, value_t **args, int argc){
    (void)visitor;(void)structure;(void)args;(void)argc;
    visitor_error(node,"pas encore de structures");
}

void visitor_free(void){
}
