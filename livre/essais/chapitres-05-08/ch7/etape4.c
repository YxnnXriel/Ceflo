// Étape 4 du chapitre 7 : listes, maps, accès, méthodes et affectations complètes.
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

// Exécute les instructions dans la portée courante.
static value_t *visitor_visit_statements(visitor_t *visitor, ast_t *node){
    for(int i = 0; i < node->list_compound->Size; i++){
        size_t mark = gc_temp_mark();
        visitor_visit(visitor,List_Get(node->list_compound,i));

        // On garde les temporaires : ils contiennent peut-être la valeur de `return`.
        if(visitor->signal != SIGNAL_NONE)
            return NULL;

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

//                          ===================================== $LIST ==============================================


static value_t *visitor_visit_list_init(visitor_t *visitor, ast_t *node){
    value_t *list = makeList();
    for(int i = 0; i < node->list_compound->Size; i++)
        Value_list_append(list,visitor_visit(visitor,List_Get(node->list_compound,i)));
    return list;
}

static value_t *visitor_visit_map_init(visitor_t *visitor, ast_t *node){
    value_t *map = makeMap();
    for(int i = 0; i < node->list_compound->Size; i += 2){
        value_t *key = visitor_visit(visitor,List_Get(node->list_compound,i));
        value_t *value = visitor_visit(visitor,List_Get(node->list_compound,i + 1));
        Value_map_set(node,map,key,value);
    }
    return map;
}

static int visitor_get_index(ast_t *node, value_t *index, int size){
    if(index->type != NUMBER)
        visitor_error(node,"un index doit être un nombre, pas %s",Value_type_name(index));

    double position = AS_NUMBER(index);
    if(position != floor(position))
        visitor_error(node,"un index doit être un entier");
    if(position < 0 || position >= size)
        visitor_error(node,"l'index %.0f n'existe pas (taille : %d)",position,size);

    return (int)position;
}

static value_t *visitor_visit_list_access(visitor_t *visitor, ast_t *node){
    value_t *container = visitor_visit(visitor,node->left);
    value_t *index = visitor_visit(visitor,node->list_ptr);

    if(container->type == LIST)
        return AS_LIST(container)->Item[visitor_get_index(node,index,AS_LIST(container)->Size)];

    if(container->type == STRING)
        return makeChar(AS_STRING(container)->str[visitor_get_index(node,index,AS_STRING(container)->lenght)]);

    if(container->type == MAP)
        return Value_map_get(node,container,index);

    visitor_error(node,"%s ne s'utilise pas avec `[ ]`",Value_type_name(container));
}

// Couleur.ROUGE, puis Couleur.ROUGE.name et Couleur.ROUGE.value
static value_t *visitor_enum_access(ast_t *node, value_t *object){
    if(object->type == ENUM){
        value_t *member = Value_enum_get(object,node->variable_name);
        if(member == NULL)
            visitor_error(node,"`%s` n'a pas de valeur `%s`",object->as.enumValue.definition->struct_definition_name,node->variable_name);
        return member;
    }

    if(strcmp(node->variable_name,"name") == 0)
        return Value_enum_item_name(object);
    if(strcmp(node->variable_name,"value") == 0)
        return Value_enum_item_value(object);
    visitor_error(node,"`%s` n'a pas d'attribut `%s`",Value_type_name(object),node->variable_name);
}

static hash_t *visitor_get_attribute(ast_t *node, value_t *object){
    if(object->type != OBJECT)
        visitor_error(node,"%s n'a pas d'attribut `%s`",Value_type_name(object),node->variable_name);

    hash_t *item = scope_get(object->as.ObjectValue.self,node->variable_name);
    if(item == NULL)
        visitor_error(node,"`%s` n'a pas d'attribut `%s`",Value_type_name(object),node->variable_name);

    return item;
}

static value_t *visitor_visit_object_access(visitor_t *visitor, ast_t *node){
    value_t *object = visitor_visit(visitor,node->left);

    if(object->type == MAP){
        value_t *value = Value_map_get_name(object,node->variable_name);
        return value != NULL ? value : makeNone();
    }

    if(object->type == ENUM || object->type == ENUM_VALUE)
        return visitor_enum_access(node,object);

    return visitor_get_attribute(node,object)->value;
}

//                          ===================================== $FUNCTION ==============================================


static value_t *visitor_visit_function_definition(visitor_t *visitor, ast_t *node){
    hash_t *existing = scope_get(visitor->scope,node->function_definition_name);
    if(existing != NULL && ((value_t*)existing->value)->type == FUNCTION)
        visitor_error(node,"la fonction `%s` est déjà définie",node->function_definition_name);

    scope_define(visitor->scope,node->function_definition_name,makeFunction(node,visitor->scope));
    return NULL;
}

static value_t *visitor_call_function(visitor_t *visitor, ast_t *node, value_t *function, value_t **args, int argc){
    ast_t *definition = function->as.functionValue.definition;
    ast_t *where = node != NULL ? node : definition;

    if(argc != definition->function_definition_args->Size){
        visitor_error(where,"`%s` prend %d argument(s), mais en reçoit %d",
            definition->function_definition_name,
            definition->function_definition_args->Size,
            argc
        );
    }

    size_t mark = gc_temp_mark();
    scope_t *caller = visitor->scope;
    visitor->scope = newScope(function->as.functionValue.closure);

    for(int i = 0; i < argc; i++){
        ast_t *arg = List_Get(definition->function_definition_args,i);
        if(arg->variable_definition_static_type_name != NULL)
            visitor_check_type(visitor,where,arg->variable_name,args[i],arg->variable_definition_static_type_name);
        hash_t *item = scope_define(visitor->scope,arg->variable_name,args[i]);
        item->type_name = arg->variable_definition_static_type_name;
    }

    visitor_visit_statements(visitor,definition->function_definition_body);

    value_t *result = visitor->signal == SIGNAL_RETURN ? visitor->return_value : makeNone();
    visitor->signal = SIGNAL_NONE;
    visitor->return_value = NULL;

    const char *return_type = definition->function_definition_return_type_name;
    if(return_type != NULL && !typer_has_type(visitor,where,result,return_type)){
        visitor_error(where,"`%s` doit retourner une valeur de type %s, pas %s",
            definition->function_definition_name,
            return_type,
            typer_type_name(result)
        );
    }

    visitor->scope = caller;

    gc_temp_reset(mark);
    gc_temp_push(result);

    return result;
}

value_t *visitor_call(visitor_t *visitor, ast_t *node, value_t *function, value_t **args, int argc){
    if(function->type == FUNCTION)
        return visitor_call_function(visitor,node,function,args,argc);

    if(function->type == BUILTIN)
        return function->as.builtinValue.function(visitor,node,args,argc);

    visitor_error(node,"%s n'est pas une fonction",Value_type_name(function));
}

// Les arguments s'évaluent de gauche à droite, avant l'appel.
#define VISIT_ARGS(visitor, node, name)                                                     \
    int name##_count = (node)->function_call_args->Size;                                    \
    value_t *name[name##_count > 0 ? name##_count : 1];                                     \
    for(int name##_i = 0; name##_i < name##_count; name##_i++)                              \
        name[name##_i] = visitor_visit(visitor,List_Get((node)->function_call_args,name##_i))

static value_t *visitor_visit_method_call(visitor_t *visitor, ast_t *node){
    ast_t *callee = node->left;
    value_t *receiver = visitor_visit(visitor,callee->left);

    // Une fonction rangée dans un map passe avant les méthodes de map.
    if(receiver->type == MAP){
        value_t *held = Value_map_get_name(receiver,callee->variable_name);
        if(held != NULL && (held->type == FUNCTION || held->type == BUILTIN)){
            VISIT_ARGS(visitor,node,args);
            return visitor_call(visitor,node,held,args,args_count);
        }
    }

    if(receiver->type == OBJECT){
        hash_t *method = scope_get(receiver->as.ObjectValue.self,callee->variable_name);
        if(method == NULL)
            visitor_error(callee,"`%s` n'a pas de méthode `%s`",Value_type_name(receiver),callee->variable_name);

        VISIT_ARGS(visitor,node,args);
        return visitor_call(visitor,node,method->value,args,args_count);
    }

    VISIT_ARGS(visitor,node,args);
    value_t *result = Value_call_method(visitor,node,receiver,callee->variable_name,args,args_count);
    if(result == NULL)
        visitor_error(callee,"%s n'a pas de méthode `%s`",Value_type_name(receiver),callee->variable_name);

    return result;
}

static value_t *visitor_visit_function_call(visitor_t *visitor, ast_t *node){
    if(node->left->type == AST_OBJ_ACCESS)
        return visitor_visit_method_call(visitor,node);

    value_t *function = visitor_visit(visitor,node->left);

    VISIT_ARGS(visitor,node,args);
    return visitor_call(visitor,node,function,args,args_count);
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

static value_t *visitor_visit_for(visitor_t *visitor, ast_t *node){
    scope_t *outer = visitor->scope;
    visitor->scope = newScope(outer);

    if(node->for_init != NULL)
        visitor_visit(visitor,node->for_init);

    size_t mark = gc_temp_mark();

    while(node->for_condition == NULL || Value_is_true(visitor_visit(visitor,node->for_condition))){
        visitor_visit(visitor,node->for_body);
        if(visitor_loop_should_stop(visitor))
            break;
        visitor_visit_statements(visitor,node->for_solve);
        gc_temp_reset(mark);
    }

    visitor->scope = outer;
    return NULL;
}

static value_t *visitor_visit_return(visitor_t *visitor, ast_t *node){
    visitor->return_value = node->return_expr != NULL ?
        visitor_visit(visitor,node->return_expr) : makeNone();
    visitor->signal = SIGNAL_RETURN;
    return NULL;
}

//                          ===================================== $MODIFIER ==============================================


/*
    La cible d'une affectation s'évalue une seule fois : dans `l[f()] += 1`,
    la fonction f n'est appelée qu'une fois.
*/
typedef struct{
    enum{ PLACE_VARIABLE, PLACE_LIST, PLACE_STRING, PLACE_MAP } kind;
    hash_t *item;
    value_t *container;
    value_t *key;       // PLACE_MAP : NULL dans `t.nom`, où la clé est le nom du noeud
    int index;
    ast_t *node;
}place_t;

static place_t visitor_get_place(visitor_t *visitor, ast_t *target){
    place_t place = {.node = target};

    if(target->type == AST_VARIABLE){
        place.kind = PLACE_VARIABLE;
        place.item = visitor_find_variable(visitor,target);
        return place;
    }

    if(target->type == AST_OBJ_ACCESS){
        value_t *object = visitor_visit(visitor,target->left);
        if(object->type == MAP){
            place.kind = PLACE_MAP;
            place.container = object;
            return place;
        }
        if(object->type == ENUM || object->type == ENUM_VALUE)
            visitor_error(target,"une énumération ne se modifie pas");
        place.kind = PLACE_VARIABLE;
        place.item = visitor_get_attribute(target,object);
        return place;
    }

    place.container = visitor_visit(visitor,target->left);
    value_t *index = visitor_visit(visitor,target->list_ptr);

    if(place.container->type == LIST){
        place.kind = PLACE_LIST;
        place.index = visitor_get_index(target,index,AS_LIST(place.container)->Size);
    }else if(place.container->type == STRING){
        place.kind = PLACE_STRING;
        place.index = visitor_get_index(target,index,AS_STRING(place.container)->lenght);
    }else if(place.container->type == MAP){
        place.kind = PLACE_MAP;
        place.key = index;
    }else{
        visitor_error(target,"%s ne s'utilise pas avec `[ ]`",Value_type_name(place.container));
    }

    return place;
}

static value_t *visitor_place_get(place_t *place){
    switch(place->kind){
        case PLACE_VARIABLE: return place->item->value;
        case PLACE_LIST: return AS_LIST(place->container)->Item[place->index];
        case PLACE_MAP:{
            if(place->key != NULL)
                return Value_map_get(place->node,place->container,place->key);
            value_t *value = Value_map_get_name(place->container,place->node->variable_name);
            return value != NULL ? value : makeNone();
        }
        default: return makeChar(AS_STRING(place->container)->str[place->index]);
    }
}

static void visitor_place_set(visitor_t *visitor, place_t *place, value_t *value);

/**
 * @brief s[i] = 'x' : une chaîne ne change jamais, alors on en construit une
 *        nouvelle et on la range à la place de l'ancienne.
 */
static void visitor_place_set_char(visitor_t *visitor, place_t *place, value_t *value){
    char carac = '\0';

    if(value->type == CHAR)
        carac = AS_CHAR(value);
    else if(value->type == NUMBER)
        carac = Value_number_to_char(AS_NUMBER(value));
    else
        visitor_error(place->node,"un caractère de chaîne reçoit un char, pas %s",Value_type_name(value));

    string_t *old = AS_STRING(place->container);
    value_t *string = makeStringLenght(old->str,old->lenght);
    AS_STRING(string)->str[place->index] = carac;

    place_t parent = visitor_get_place(visitor,place->node->left);
    visitor_place_set(visitor,&parent,string);
}

static void visitor_place_set(visitor_t *visitor, place_t *place, value_t *value){
    switch(place->kind){
        case PLACE_VARIABLE:
            if(place->item->is_const)
                visitor_error(place->node,"`%s` est une constante",place->item->key);
            if(place->item->type_name != NULL)
                visitor_check_type(visitor,place->node,place->item->key,value,place->item->type_name);
            place->item->value = value;
            break;
        case PLACE_LIST:
            // La liste a pu rétrécir pendant le calcul de la valeur.
            if(place->index >= AS_LIST(place->container)->Size)
                visitor_error(place->node,"l'index %d n'existe plus",place->index);
            AS_LIST(place->container)->Item[place->index] = value;
            break;
        case PLACE_STRING:
            visitor_place_set_char(visitor,place,value);
            break;
        case PLACE_MAP:
            if(place->key != NULL)
                Value_map_set(place->node,place->container,place->key,value);
            else
                Value_map_set_name(place->container,place->node->variable_name,value);
            break;
    }
}

static value_t *visitor_visit_assignement(visitor_t *visitor, ast_t *node){
    place_t place = visitor_get_place(visitor,node->assignement_left);
    value_t *value = visitor_visit(visitor,node->assignement_value);

    if(node->op != TOKEN_EQUAL){
        value_t *current = visitor_place_get(&place);
        gc_temp_push(current);
        value = value_expr(visitor,node,current,node->op,value);
    }

    visitor_place_set(visitor,&place,value);
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
        case AST_LFUNC: return makeFunction(node,visitor->scope);
        case AST_FUNCTION_DEFINITION: return visitor_visit_function_definition(visitor,node);
        case AST_RETURN: return visitor_visit_return(visitor,node);
        case AST_BREAK: visitor->signal = SIGNAL_BREAK; return NULL;
        case AST_CONTINUE: visitor->signal = SIGNAL_CONTINUE; return NULL;
        case AST_FOR: return visitor_visit_for(visitor,node);
        case AST_LIST: return visitor_visit_list_init(visitor,node);
        case AST_MAP: return visitor_visit_map_init(visitor,node);
        case AST_OBJ_ACCESS: return visitor_visit_object_access(visitor,node);
        case AST_LIST_ACCESS: return visitor_visit_list_access(visitor,node);
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
