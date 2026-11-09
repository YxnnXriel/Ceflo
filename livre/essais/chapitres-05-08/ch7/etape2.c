// Étape 2 du chapitre 7 : les blocs, if, while et l'affectation.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

// Un bloc { } : il a sa propre portée dès qu'il déclare un nom.
static value_t *visitor_visit_compound(visitor_t *visitor, ast_t *node){
    if(!node->has_definition)
        return visitor_visit_statements(visitor,node);

    scope_t *outer = visitor->scope;
    visitor->scope = newScope(outer);
    visitor_visit_statements(visitor,node);
    visitor->scope = outer;
    return NULL;
}

static void visitor_check_type(visitor_t *visitor, ast_t *node, const char *name, value_t *value, const char *type_name){
    if(!typer_has_type(visitor,node,value,type_name))
        visitor_error(node,"`%s` est de type %s, pas %s",name,type_name,typer_type_name(value));
}

// Une variable typée garde son type, même si on la déclare une deuxième fois.
static const char *visitor_definition_type(visitor_t *visitor, ast_t *node){
    const char *type_name = node->variable_definition_static_type_name;
    hash_t *old = scope_get(visitor->scope,node->variable_definition_name);

    if(old == NULL || old->type_name == NULL)
        return type_name;
    if(type_name != NULL && strcmp(type_name,old->type_name) != 0)
        visitor_error(node,"`%s` est déjà de type %s",node->variable_definition_name,old->type_name);
    return old->type_name;
}

static value_t *visitor_visit_variable_definition(visitor_t *visitor, ast_t *node){
    for(; node != NULL; node = node->left){
        const char *type_name = visitor_definition_type(visitor,node);
        value_t *value = NULL;

        if(node->variable_definition_value != NULL){
            value = visitor_visit(visitor,node->variable_definition_value);
            if(type_name != NULL)
                visitor_check_type(visitor,node,node->variable_definition_name,value,type_name);
        }else{
            value = type_name != NULL ? typer_default_value(visitor,node,type_name) : makeNone();
        }

        hash_t *item = scope_define(visitor->scope,node->variable_definition_name,value);
        item->type_name = type_name;
        item->is_const = node->is_const;
    }
    return NULL;
}

// Le hash du nom ne se calcule qu'une fois par noeud, et non à chaque lecture.
static hash_t *visitor_find_variable(visitor_t *visitor, ast_t *node){
    if(node->variable_hash == 0)
        node->variable_hash = Hash_Name(node->variable_name);

    hash_t *item = scope_lookup_hashed(visitor->scope,node->variable_name,node->variable_hash);
    if(item == NULL)
        visitor_error(node,"`%s` n'est pas définie",node->variable_name);
    return item;
}

static value_t *visitor_visit_variable(visitor_t *visitor, ast_t *node){
    return visitor_find_variable(visitor,node)->value;
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

// `and` et `or` n'évaluent leur côté droit que s'il est utile.
static value_t *visitor_visit_binop(visitor_t *visitor, ast_t *node){
    value_t *left = visitor_visit(visitor,node->left);

    if(node->op == TOKEN_AND){
        if(!Value_is_true(left))
            return makeBool(false);
        return makeBool(Value_is_true(visitor_visit(visitor,node->right)));
    }

    if(node->op == TOKEN_OR){
        if(Value_is_true(left))
            return makeBool(true);
        return makeBool(Value_is_true(visitor_visit(visitor,node->right)));
    }

    value_t *right = visitor_visit(visitor,node->right);
    return value_expr(visitor,node,left,node->op,right);
}

static value_t *visitor_visit_unop(visitor_t *visitor, ast_t *node){
    value_t *value = visitor_visit(visitor,node->right);

    if(node->op == TOKEN_NOT)
        return makeBool(!Value_is_true(value));

    if(value->type != NUMBER && value->type != BOOL && value->type != CHAR)
        visitor_error(node,"l'opérateur `-` ne s'applique pas à %s",Value_type_name(value));

    return makeNumber(-Value_as_double(value));
}

static value_t *visitor_visit_string(ast_t *node){
    return makeStringLenght(node->string_value,node->string_lenght);
}

static value_t *visitor_visit_if(visitor_t *visitor, ast_t *node){
    if(Value_is_true(visitor_visit(visitor,node->if_expr)))
        return visitor_visit(visitor,node->if_body);
    if(node->if_other != NULL)
        return visitor_visit(visitor,node->if_other);
    if(node->else_body != NULL)
        return visitor_visit(visitor,node->else_body);
    return NULL;
}

static value_t *visitor_visit_ternary(visitor_t *visitor, ast_t *node){
    return Value_is_true(visitor_visit(visitor,node->if_expr)) ?
        visitor_visit(visitor,node->if_other) :
        visitor_visit(visitor,node->else_body);
}

// Après le corps d'une boucle : retourne true si la boucle doit s'arrêter.
static bool visitor_loop_should_stop(visitor_t *visitor){
    if(visitor->signal == SIGNAL_BREAK){
        visitor->signal = SIGNAL_NONE;
        return true;
    }
    if(visitor->signal == SIGNAL_CONTINUE)
        visitor->signal = SIGNAL_NONE;
    return visitor->signal == SIGNAL_RETURN;
}

static value_t *visitor_visit_while(visitor_t *visitor, ast_t *node){
    size_t mark = gc_temp_mark();

    while(Value_is_true(visitor_visit(visitor,node->while_expr))){
        visitor_visit(visitor,node->while_body);
        if(visitor_loop_should_stop(visitor))
            return NULL;
        gc_temp_reset(mark);
    }

    return NULL;
}

// Version simple : seule une variable peut recevoir une valeur (7.6 fera le reste).
static value_t *visitor_visit_assignement(visitor_t *visitor, ast_t *node){
    if(node->assignement_left->type != AST_VARIABLE)
        visitor_error(node,"pas encore : seule une variable peut être affectée");

    hash_t *item = visitor_find_variable(visitor,node->assignement_left);
    value_t *value = visitor_visit(visitor,node->assignement_value);
    if(node->op != TOKEN_EQUAL)
        value = value_expr(visitor,node,item->value,node->op,value);

    if(item->is_const)
        visitor_error(node,"`%s` est une constante",item->key);
    item->value = value;
    return NULL;
}

static value_t *visitor_dispatch(visitor_t *visitor, ast_t *node){
    switch(node->type){
        case AST_NOOP: return NULL;
        case AST_COMPOUND: return visitor_visit_compound(visitor,node);
        case AST_NONE: return makeNone();
        case AST_BOOL: return makeBool(node->bool_value);
        case AST_CHAR: return makeChar(node->char_value);
        case AST_NUMBER: return makeNumber(node->number_value);
        case AST_STRING: return visitor_visit_string(node);
        case AST_VARIABLE: return visitor_visit_variable(visitor,node);
        case AST_FUNCTION_CALL: return visitor_visit_function_call(visitor,node);
        case AST_BINOP: return visitor_visit_binop(visitor,node);
        case AST_UNOP: return visitor_visit_unop(visitor,node);
        case AST_TERNARY: return visitor_visit_ternary(visitor,node);
        case AST_VARIABLE_DEFINITION: return visitor_visit_variable_definition(visitor,node);
        case AST_ASSIGNEMENT: return visitor_visit_assignement(visitor,node);
        case AST_IF: return visitor_visit_if(visitor,node);
        case AST_WHILE: return visitor_visit_while(visitor,node);
        default:
            visitor_error(node,"ce noeud n'est pas encore exécuté (type %d)",node->type);
    }
}

/**
 * @brief visite un noeud. Sa valeur reste en vie jusqu'à la fin de l'instruction :
 *        dans `println(x, change())`, change() peut réaffecter x pendant que
 *        l'ancienne valeur attend son tour pour être affichée.
 */
value_t *visitor_visit(visitor_t *visitor, ast_t *node){
    if(ceflo_stack_exceeded())
        visitor_error(node,"trop d'appels imbriqués : la récursion ne s'arrête peut-être jamais");

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

value_t *visitor_new_object(visitor_t *visitor, ast_t *node, value_t *structure, value_t **args, int argc){
    (void)visitor;(void)structure;(void)args;(void)argc;
    visitor_error(node,"pas encore de structures");
}

void visitor_free(void){
}
