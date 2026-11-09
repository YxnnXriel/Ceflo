/**
 * @file parser.c
 * @author Yann Ariel (yannariel13@gmail.com)
 * @brief analyse descendante : une fonction par règle de GRAMMAIRE.md
 * @copyright Copyright (c) 2023
 */

#include <stdlib.h>
#include <string.h>

#include "include/parser.h"
#include "include/error.h"
#include "include/str.h"

#define TYPE(parser) ((parser)->current.type)
#define VALUE(parser) ((parser)->current.value)

static ast_t *parser_parse_block(parser_t *parser);
static ast_t *parser_parse_expr(parser_t *parser);


//                          ===================================== $OUTILS ==============================================


static _Noreturn void parser_error_expected(parser_t *parser, const char *expected){
    error_syntax(
        parser->lexer->fileName,
        parser->current.line,
        parser->current.colone,
        "`%s` attendu, mais `%s` trouvé",
        expected,
        token_to_string(TYPE(parser))
    );
}

static _Noreturn void parser_error_here(parser_t *parser, const char *message){
    error_syntax(parser->lexer->fileName,parser->current.line,parser->current.colone,"%s",message);
}

static void newParser(parser_t *parser, lexer_t *lexer){
    memset(parser,0,sizeof(*parser));
    parser->lexer = lexer;
    parser->prev.type = TOKEN_EOF;
    parser->current = lexer_get_next_token(lexer);
}

static void freeParser(parser_t *parser){
    freeToken(&parser->prev);
    freeToken(&parser->current);
}

/**
 * @brief passe au jeton suivant : l'ancien jeton reste lisible dans parser->prev
 */
static void parser_advance(parser_t *parser){
    freeToken(&parser->prev);
    parser->prev = parser->current;
    parser->current = lexer_get_next_token(parser->lexer);
}

static void parser_eat(parser_t *parser, TokenEnum type){
    if(TYPE(parser) != type)
        parser_error_expected(parser,token_to_string(type));
    parser_advance(parser);
}

static bool parser_match(parser_t *parser, TokenEnum type){
    if(TYPE(parser) != type)
        return false;
    parser_advance(parser);
    return true;
}

static ast_t *parser_new_node(parser_t *parser, ast_type type){
    return newAstWithLine(type,parser->lexer->fileName,parser->current.line,parser->current.colone);
}

static void parser_check_depth(parser_t *parser){
    if(ceflo_stack_exceeded())
        parser_error_here(parser,"code trop profondément imbriqué");
}

/**
 * @brief lit un nom et en donne une copie
 */
static char *parser_eat_id(parser_t *parser){
    char *name = NULL;
    if(TYPE(parser) != TOKEN_ID)
        parser_error_expected(parser,"identifiant");
    allowChar(&name,VALUE(parser));
    parser_advance(parser);
    return name;
}


//                          ===================================== $EXPRESSIONS ==========================================


static list_t *parser_parse_call_args(parser_t *parser){
    list_t *args = newList();

    parser_eat(parser,TOKEN_LPARENT);
    if(TYPE(parser) != TOKEN_RPARENT){
        do{
            List_Append(args,parser_parse_expr(parser));
        }while(parser_match(parser,TOKEN_COMA));
    }
    parser_eat(parser,TOKEN_RPARENT);

    return args;
}

static ast_t *parser_parse_number(parser_t *parser){
    ast_t *number = parser_new_node(parser,AST_NUMBER);
    number->number_value = strtod(VALUE(parser),NULL);
    parser_advance(parser);
    return number;
}

static ast_t *parser_parse_string(parser_t *parser){
    ast_t *string = parser_new_node(parser,AST_STRING);
    string->string_lenght = parser->current.lenght;
    string->string_value = ceflo_alloc((size_t)string->string_lenght + 1);
    memcpy(string->string_value,VALUE(parser),(size_t)string->string_lenght);
    parser_advance(parser);
    return string;
}

static ast_t *parser_parse_char(parser_t *parser){
    ast_t *carac = parser_new_node(parser,AST_CHAR);
    carac->char_value = VALUE(parser)[0];
    parser_advance(parser);
    return carac;
}

static ast_t *parser_parse_bool(parser_t *parser){
    ast_t *boolean = parser_new_node(parser,AST_BOOL);
    boolean->bool_value = TYPE(parser) == TOKEN_TRUE;
    parser_advance(parser);
    return boolean;
}

static ast_t *parser_parse_list(parser_t *parser){
    ast_t *list = parser_new_node(parser,AST_LIST);
    list->list_compound = newList();

    parser_eat(parser,TOKEN_LBRACE);
    if(TYPE(parser) != TOKEN_RBRACE){
        do{
            List_Append(list->list_compound,parser_parse_expr(parser));
        }while(parser_match(parser,TOKEN_COMA));
    }
    parser_eat(parser,TOKEN_RBRACE);

    return list;
}

// new Nom(args)  ou  new Nom
static ast_t *parser_parse_new(parser_t *parser){
    ast_t *node = parser_new_node(parser,AST_STRUCT);
    parser_eat(parser,TOKEN_NEW);
    node->function_call_name = parser_eat_id(parser);
    node->function_call_args = TYPE(parser) == TOKEN_LPARENT && !parser->current.newline_before ?
        parser_parse_call_args(parser) : newList();
    return node;
}

// @type(args)
static ast_t *parser_parse_type_call(parser_t *parser){
    ast_t *node = parser_new_node(parser,AST_TYPE_CALL);
    parser_eat(parser,TOKEN_AT_SIGN);
    node->function_call_name = parser_eat_id(parser);
    node->function_call_args = parser_parse_call_args(parser);
    return node;
}

static ast_t *parser_string_from_id(parser_t *parser){
    ast_t *string = parser_new_node(parser,AST_STRING);
    string->string_lenght = (int)strlen(VALUE(parser));
    allowChar(&string->string_value,VALUE(parser));
    return string;
}

/**
 * @brief une entrée de map, comme en TypeScript :
 *        { nom: 1, "avec espace": 2, 3: 4, [calcul]: 5, raccourci }
 */
static void parser_parse_map_entry(parser_t *parser, list_t *items){
    ast_t *key = NULL;

    if(TYPE(parser) == TOKEN_ID){
        key = parser_string_from_id(parser);
        parser_advance(parser);

        // { raccourci } veut dire { raccourci: raccourci }
        if(TYPE(parser) == TOKEN_COMA || TYPE(parser) == TOKEN_RBRACKET){
            ast_t *variable = newAstWithLine(AST_VARIABLE,key->file,key->line,key->colone);
            allowChar(&variable->variable_name,key->string_value);
            List_Append(items,key);
            List_Append(items,variable);
            return;
        }
    }else if(TYPE(parser) == TOKEN_STRING){
        key = parser_parse_string(parser);
    }else if(TYPE(parser) == TOKEN_INTEGER || TYPE(parser) == TOKEN_FLOAT){
        key = parser_parse_number(parser);
    }else if(TYPE(parser) == TOKEN_LBRACE){
        parser_advance(parser);
        key = parser_parse_expr(parser);
        parser_eat(parser,TOKEN_RBRACE);
    }else{
        parser_error_expected(parser,"clé");
    }

    parser_eat(parser,TOKEN_COLON);
    List_Append(items,key);
    List_Append(items,parser_parse_expr(parser));
}

static ast_t *parser_parse_map(parser_t *parser){
    ast_t *map = parser_new_node(parser,AST_MAP);
    map->list_compound = newList();

    parser_eat(parser,TOKEN_LBRACKET);
    while(TYPE(parser) != TOKEN_RBRACKET){
        parser_parse_map_entry(parser,map->list_compound);
        if(!parser_match(parser,TOKEN_COMA))
            break;
    }
    parser_eat(parser,TOKEN_RBRACKET);

    return map;
}

static ast_t *parser_parse_primary(parser_t *parser){
    ast_t *node = NULL;

    switch(TYPE(parser)){
        case TOKEN_INTEGER:
        case TOKEN_FLOAT:
            return parser_parse_number(parser);
        case TOKEN_STRING:
            return parser_parse_string(parser);
        case TOKEN_CHAR:
            return parser_parse_char(parser);
        case TOKEN_TRUE:
        case TOKEN_FALSE:
            return parser_parse_bool(parser);
        case TOKEN_NONE:
            node = parser_new_node(parser,AST_NONE);
            parser_advance(parser);
            return node;
        case TOKEN_ID:
            node = parser_new_node(parser,AST_VARIABLE);
            node->variable_name = parser_eat_id(parser);
            return node;
        case TOKEN_LBRACE:
            return parser_parse_list(parser);
        case TOKEN_LBRACKET:
            return parser_parse_map(parser);
        case TOKEN_NEW:
            return parser_parse_new(parser);
        case TOKEN_AT_SIGN:
            return parser_parse_type_call(parser);
        case TOKEN_LPARENT:
            parser_advance(parser);
            node = parser_parse_expr(parser);
            parser_eat(parser,TOKEN_RPARENT);
            return node;
        default:
            parser_error_expected(parser,"expression");
    }
}

/**
 * @brief les suites `.nom`, `[index]` et `(args)`
 *
 * Comme les `;` sont facultatifs, `[` et `(` doivent rester sur la même ligne :
 * sinon `println(a)` suivi de `[1,2].print()` serait lu comme un accès à la liste.
 */
static ast_t *parser_parse_postfix(parser_t *parser){
    ast_t *value = parser_parse_primary(parser);

    while(true){
        if(TYPE(parser) == TOKEN_DOT){
            ast_t *access = parser_new_node(parser,AST_OBJ_ACCESS);
            parser_advance(parser);
            access->left = value;
            access->colone = parser->current.colone;
            access->line = parser->current.line;
            access->variable_name = parser_eat_id(parser);
            value = access;
        }else if(TYPE(parser) == TOKEN_LBRACE && !parser->current.newline_before){
            ast_t *access = parser_new_node(parser,AST_LIST_ACCESS);
            parser_advance(parser);
            access->left = value;
            access->list_ptr = parser_parse_expr(parser);
            parser_eat(parser,TOKEN_RBRACE);
            value = access;
        }else if(TYPE(parser) == TOKEN_LPARENT && !parser->current.newline_before){
            ast_t *call = parser_new_node(parser,AST_FUNCTION_CALL);
            call->left = value;
            call->function_call_args = parser_parse_call_args(parser);
            value = call;
        }else{
            return value;
        }
    }
}

static ast_t *parser_parse_factor(parser_t *parser){
    parser_check_depth(parser);

    if(TYPE(parser) == TOKEN_MINUS || TYPE(parser) == TOKEN_NOT){
        ast_t *unop = parser_new_node(parser,AST_UNOP);
        unop->op = TYPE(parser);
        parser_advance(parser);
        unop->right = parser_parse_factor(parser);
        return unop;
    }

    return parser_parse_postfix(parser);
}

static ast_t *parser_new_binop(parser_t *parser, ast_t *left){
    ast_t *binop = parser_new_node(parser,AST_BINOP);
    binop->left = left;
    binop->op = TYPE(parser);
    parser_advance(parser);
    return binop;
}

static ast_t *parser_parse_term(parser_t *parser){
    ast_t *value = parser_parse_factor(parser);

    while(TYPE(parser) == TOKEN_MULT || TYPE(parser) == TOKEN_DIV || TYPE(parser) == TOKEN_MODULO){
        ast_t *binop = parser_new_binop(parser,value);
        binop->right = parser_parse_factor(parser);
        value = binop;
    }

    return value;
}

static ast_t *parser_parse_expression(parser_t *parser){
    ast_t *value = parser_parse_term(parser);

    while(TYPE(parser) == TOKEN_PLUS || TYPE(parser) == TOKEN_MINUS){
        ast_t *binop = parser_new_binop(parser,value);
        binop->right = parser_parse_term(parser);
        value = binop;
    }

    return value;
}

static ast_t *parser_parse_comparison(parser_t *parser){
    ast_t *value = parser_parse_expression(parser);

    while(TYPE(parser) == TOKEN_SUP || TYPE(parser) == TOKEN_SUP_EQUAL ||
          TYPE(parser) == TOKEN_INF || TYPE(parser) == TOKEN_INF_EQUAL){
        ast_t *binop = parser_new_binop(parser,value);
        binop->right = parser_parse_expression(parser);
        value = binop;
    }

    return value;
}

static ast_t *parser_parse_equality(parser_t *parser){
    ast_t *value = parser_parse_comparison(parser);

    while(TYPE(parser) == TOKEN_EQUAL_EQUAL || TYPE(parser) == TOKEN_DIF){
        ast_t *binop = parser_new_binop(parser,value);
        binop->right = parser_parse_comparison(parser);
        value = binop;
    }

    return value;
}

static ast_t *parser_parse_and(parser_t *parser){
    ast_t *value = parser_parse_equality(parser);

    while(TYPE(parser) == TOKEN_AND){
        ast_t *binop = parser_new_binop(parser,value);
        binop->right = parser_parse_equality(parser);
        value = binop;
    }

    return value;
}

static ast_t *parser_parse_or(parser_t *parser){
    ast_t *value = parser_parse_and(parser);

    while(TYPE(parser) == TOKEN_OR){
        ast_t *binop = parser_new_binop(parser,value);
        binop->right = parser_parse_and(parser);
        value = binop;
    }

    return value;
}

// condition ? alors : sinon     (sans `: sinon`, la valeur vaut none)
static ast_t *parser_parse_expr(parser_t *parser){
    ast_t *value = parser_parse_or(parser);

    if(TYPE(parser) != TOKEN_MARK)
        return value;

    ast_t *ternary = parser_new_node(parser,AST_TERNARY);
    parser_advance(parser);
    ternary->if_expr = value;
    ternary->if_other = parser_parse_expr(parser);
    ternary->else_body = parser_match(parser,TOKEN_COLON) ?
        parser_parse_expr(parser) : parser_new_node(parser,AST_NONE);

    return ternary;
}


//                          ===================================== $DEFINITIONS ==========================================


// x::type
static char *parser_parse_static_type(parser_t *parser){
    if(TYPE(parser) == TOKEN_NONE)
        parser_error_here(parser,"`none` ne peut pas être le type d'une variable");
    return parser_eat_id(parser);
}

// var a = 1, b::int = 2    /    const K = 3
static ast_t *parser_parse_variable_definition(parser_t *parser){
    bool is_const = TYPE(parser) == TOKEN_CONST;
    parser_advance(parser);

    ast_t *first = NULL;
    ast_t *last = NULL;

    do{
        ast_t *var = parser_new_node(parser,AST_VARIABLE_DEFINITION);
        var->is_const = is_const;
        var->variable_definition_name = parser_eat_id(parser);

        if(parser_match(parser,TOKEN_DOUBLE_COLON))
            var->variable_definition_static_type_name = parser_parse_static_type(parser);

        if(parser_match(parser,TOKEN_EQUAL)){
            var->variable_definition_value = parser_parse_expr(parser);
        }else if(is_const){
            parser_error_here(parser,"une constante doit recevoir une valeur");
        }

        if(first == NULL)
            first = var;
        else
            last->left = var;
        last = var;
    }while(parser_match(parser,TOKEN_COMA));

    return first;
}

static ast_t *parser_parse_if(parser_t *parser){
    ast_t *ast_if = parser_new_node(parser,AST_IF);
    parser_advance(parser);

    ast_if->if_expr = parser_parse_expr(parser);
    ast_if->if_body = parser_parse_block(parser);

    if(parser_match(parser,TOKEN_ELSE)){
        if(TYPE(parser) == TOKEN_IF)
            ast_if->if_other = parser_parse_if(parser);
        else
            ast_if->else_body = parser_parse_block(parser);
    }

    return ast_if;
}

static ast_t *parser_parse_while(parser_t *parser){
    ast_t *ast_while = parser_new_node(parser,AST_WHILE);
    parser_advance(parser);

    ast_while->while_expr = parser_parse_expr(parser);

    parser->loop_depth += 1;
    ast_while->while_body = parser_parse_block(parser);
    parser->loop_depth -= 1;

    return ast_while;
}

static ast_t *parser_parse_assignement(parser_t *parser);

// for init ; condition ; action, action { }     (chaque partie peut être vide)
/**
 * @brief for var a in c { }  et  for var a, b in c { }
 *        Les variables sont déjà lues : `var a, b` commence aussi un `for` classique.
 */
static ast_t *parser_parse_for_in(parser_t *parser, ast_t *ast_for){
    ast_t *first = ast_for->for_init;
    ast_t *second = first->left;

    if(first->variable_definition_value != NULL || (second != NULL && second->variable_definition_value != NULL))
        parser_error_here(parser,"les variables de `for ... in` ne reçoivent pas de valeur");
    if(second != NULL && second->left != NULL)
        parser_error_here(parser,"`for ... in` prend une ou deux variables");

    ast_for->type = AST_FOR_IN;
    parser_advance(parser);
    ast_for->for_condition = parser_parse_expr(parser);

    parser->loop_depth += 1;
    ast_for->for_body = parser_parse_block(parser);
    parser->loop_depth -= 1;

    return ast_for;
}

static ast_t *parser_parse_for(parser_t *parser){
    ast_t *ast_for = parser_new_node(parser,AST_FOR);
    parser_advance(parser);

    if(TYPE(parser) != TOKEN_SEMI){
        ast_for->for_init = TYPE(parser) == TOKEN_VAR ?
            parser_parse_variable_definition(parser) : parser_parse_assignement(parser);
    }

    if(TYPE(parser) == TOKEN_IN && ast_for->for_init->type == AST_VARIABLE_DEFINITION)
        return parser_parse_for_in(parser,ast_for);

    parser_eat(parser,TOKEN_SEMI);

    if(TYPE(parser) != TOKEN_SEMI)
        ast_for->for_condition = parser_parse_expr(parser);
    parser_eat(parser,TOKEN_SEMI);

    ast_for->for_solve = parser_new_node(parser,AST_COMPOUND);
    ast_for->for_solve->list_compound = newList();
    if(TYPE(parser) != TOKEN_LBRACKET){
        do{
            List_Append(ast_for->for_solve->list_compound,parser_parse_assignement(parser));
        }while(parser_match(parser,TOKEN_COMA));
    }

    parser->loop_depth += 1;
    ast_for->for_body = parser_parse_block(parser);
    parser->loop_depth -= 1;

    return ast_for;
}

static ast_t *parser_parse_loop_jump(parser_t *parser, ast_type type){
    if(parser->loop_depth == 0)
        parser_error_here(parser,"`break` et `continue` s'utilisent dans une boucle");

    ast_t *jump = parser_new_node(parser,type);
    parser_advance(parser);
    return jump;
}

static bool parser_is_assign_target(ast_t *node){
    return node->type == AST_VARIABLE || node->type == AST_OBJ_ACCESS || node->type == AST_LIST_ACCESS;
}

static bool parser_get_assign_op(TokenEnum type, TokenEnum *op){
    switch(type){
        case TOKEN_EQUAL: *op = TOKEN_EQUAL; return true;
        case TOKEN_PLUS_EQUAL: *op = TOKEN_PLUS; return true;
        case TOKEN_MINUS_EQUAL: *op = TOKEN_MINUS; return true;
        case TOKEN_MULT_EQUAL: *op = TOKEN_MULT; return true;
        case TOKEN_DIV_EQUAL: *op = TOKEN_DIV; return true;
        case TOKEN_MODULO_EQUAL: *op = TOKEN_MODULO; return true;
        default: return false;
    }
}

// expression   ou   cible = expression   ou   cible += expression
static ast_t *parser_parse_assignement(parser_t *parser){
    ast_t *value = parser_parse_expr(parser);
    TokenEnum op;

    if(!parser_get_assign_op(TYPE(parser),&op))
        return value;

    if(!parser_is_assign_target(value))
        parser_error_here(parser,"on ne peut pas affecter une valeur à cette expression");

    ast_t *assign = parser_new_node(parser,AST_ASSIGNEMENT);
    parser_advance(parser);
    assign->op = op;
    assign->assignement_left = value;
    assign->assignement_value = parser_parse_expr(parser);

    return assign;
}

// Étape 3 : les fonctions, les structures, `match` et les modules viendront à l'étape 4.
static ast_t *parser_parse_statment(parser_t *parser){
    switch(TYPE(parser)){
        case TOKEN_VAR:
        case TOKEN_CONST: return parser_parse_variable_definition(parser);
        case TOKEN_IF: return parser_parse_if(parser);
        case TOKEN_WHILE: return parser_parse_while(parser);
        case TOKEN_FOR: return parser_parse_for(parser);
        case TOKEN_BREAK: return parser_parse_loop_jump(parser,AST_BREAK);
        case TOKEN_CONTINUE: return parser_parse_loop_jump(parser,AST_CONTINUE);
        default: return parser_parse_assignement(parser);
    }
}

static bool parser_is_definition(ast_t *node){
    return node->type == AST_VARIABLE_DEFINITION || node->type == AST_FUNCTION_DEFINITION ||
        node->type == AST_STRUCT_DEFINITION || node->type == AST_ENUM_DEFINITION || node->type == AST_INCLUDE;
}

/**
 * @brief les instructions, jusqu'à `}` ou la fin du fichier
 */
static ast_t *parser_parse_statments(parser_t *parser){
    ast_t *block = parser_new_node(parser,AST_COMPOUND);
    block->list_compound = newList();

    while(TYPE(parser) != TOKEN_RBRACKET && TYPE(parser) != TOKEN_EOF){
        if(parser_match(parser,TOKEN_SEMI))
            continue;

        ast_t *child = parser_parse_statment(parser);
        block->has_definition = block->has_definition || parser_is_definition(child);
        List_Append(block->list_compound,child);
    }

    return block;
}

static ast_t *parser_parse_block(parser_t *parser){
    parser_check_depth(parser);

    parser_eat(parser,TOKEN_LBRACKET);
    parser->block_depth += 1;
    ast_t *block = parser_parse_statments(parser);
    parser->block_depth -= 1;
    parser_eat(parser,TOKEN_RBRACKET);

    return block;
}

ast_t *parser_parse(lexer_t *lexer){
    parser_t parser;
    newParser(&parser,lexer);

    ast_t *node = parser_parse_statments(&parser);
    if(TYPE(&parser) != TOKEN_EOF)
        parser_error_here(&parser,"`}` en trop");

    freeParser(&parser);
    return node;
}
