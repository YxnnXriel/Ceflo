# Assemble etape2.c et etape3.c à partir des vraies fonctions de src/visitor.c
src = open("../../../src/visitor.c").read().split("\n")
def L(a, b): return "\n".join(src[a-1:b]) + "\n\n"

entete = '''#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "visitor.h"
#include "error.h"
#include "parser.h"
#include "value_expr.h"
#include "typer.h"
#include "bulletin.h"

'''
assign_simple = '''// Version simple : seule une variable peut recevoir une valeur (7.6 fera le reste).
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

'''
appel_simple = '''value_t *visitor_call(visitor_t *visitor, ast_t *node, value_t *function, value_t **args, int argc){
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

'''
fin = '''int visitor_run_file(visitor_t *visitor, int argc, char **argv){
    memset(visitor,0,sizeof(*visitor));
    visitor->builtins = newScope(NULL);
    gc_root(visitor->builtins);
    bulletin_define_all(visitor->builtins,argc,argv);

    lexer_t lexer;
    if(!newLexer(&lexer,argv[0])){
        fprintf(stderr,"erreur: impossible d'ouvrir `%s`\\n",argv[0]);
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
'''
def dispatch(cases):
    return ("static value_t *visitor_dispatch(visitor_t *visitor, ast_t *node){\n    switch(node->type){\n"
        + "".join("        " + c + "\n" for c in cases)
        + '        default:\n            visitor_error(node,"ce noeud n\'est pas encore exécuté (type %d)",node->type);\n    }\n}\n\n')

base = ['case AST_NOOP: return NULL;','case AST_COMPOUND: return visitor_visit_compound(visitor,node);',
 'case AST_NONE: return makeNone();','case AST_BOOL: return makeBool(node->bool_value);',
 'case AST_CHAR: return makeChar(node->char_value);','case AST_NUMBER: return makeNumber(node->number_value);',
 'case AST_STRING: return visitor_visit_string(node);',
 'case AST_VARIABLE: return visitor_visit_variable(visitor,node);',
 'case AST_FUNCTION_CALL: return visitor_visit_function_call(visitor,node);',
 'case AST_BINOP: return visitor_visit_binop(visitor,node);','case AST_UNOP: return visitor_visit_unop(visitor,node);',
 'case AST_TERNARY: return visitor_visit_ternary(visitor,node);',
 'case AST_VARIABLE_DEFINITION: return visitor_visit_variable_definition(visitor,node);',
 'case AST_ASSIGNEMENT: return visitor_visit_assignement(visitor,node);',
 'case AST_IF: return visitor_visit_if(visitor,node);','case AST_WHILE: return visitor_visit_while(visitor,node);']
visit = L(1066, 1080)
proto = "value_t *visitor_visit(visitor_t *visitor, ast_t *node);\n\n"

stmt_simple = """static value_t *visitor_visit_statements(visitor_t *visitor, ast_t *node){
    for(int i = 0; i < node->list_compound->Size; i++){
        size_t mark = gc_temp_mark();
        visitor_visit(visitor,List_Get(node->list_compound,i));
        gc_temp_reset(mark);
    }
    return NULL;
}

"""
e2 = ("// Étape 2 du chapitre 7 : les blocs, if, while et l'affectation.\n" + entete
      + stmt_simple + L(60,70) + L(76,126) + appel_simple + L(436,470) + L(476,484) + L(511,539)
      + assign_simple + dispatch(base) + visit + fin)
open("etape2.c","w").write(e2)

e3cases = base + ['case AST_LFUNC: return makeFunction(node,visitor->scope);',
 'case AST_FUNCTION_DEFINITION: return visitor_visit_function_definition(visitor,node);',
 'case AST_RETURN: return visitor_visit_return(visitor,node);',
 'case AST_BREAK: visitor->signal = SIGNAL_BREAK; return NULL;',
 'case AST_CONTINUE: visitor->signal = SIGNAL_CONTINUE; return NULL;',
 'case AST_FOR: return visitor_visit_for(visitor,node);']
e3 = ("// Étape 3 du chapitre 7 : les fonctions, return, break, continue et for.\n" + entete
      + L(45,70) + L(76,126) + L(300,374) + L(406,414).replace(
        "    if(node->left->type == AST_OBJ_ACCESS)\n        return visitor_visit_method_call(visitor,node);\n\n","")
      + L(436,470) + L(476,484) + L(511,560) + L(664,669)
      + assign_simple + dispatch(e3cases) + visit + fin)
open("etape3.c","w").write(e3)

e4cases = e3cases + ['case AST_LIST: return visitor_visit_list_init(visitor,node);',
 'case AST_MAP: return visitor_visit_map_init(visitor,node);',
 'case AST_OBJ_ACCESS: return visitor_visit_object_access(visitor,node);',
 'case AST_LIST_ACCESS: return visitor_visit_list_access(visitor,node);']
corps4 = (L(45,70) + L(76,126) + L(129,176) + L(205,219) + L(274,297) + L(300,414)
      + L(436,470) + L(476,484) + L(511,560) + L(664,669) + L(672,807))
e4 = ("// Étape 4 du chapitre 7 : listes, maps, accès, méthodes et affectations complètes.\n"
      + entete + corps4 + dispatch(e4cases) + visit + fin)
open("etape4.c","w").write(e4)

e5cases = e4cases + ['case AST_STRUCT: return visitor_visit_new(visitor,node);',
 'case AST_TYPE_CALL: return visitor_visit_type_call(visitor,node);',
 'case AST_STRUCT_DEFINITION: return visitor_visit_struct_definition(visitor,node);',
 'case AST_ENUM_DEFINITION: return visitor_visit_enum_definition(visitor,node);',
 'case AST_MATCH: return visitor_visit_match(visitor,node);',
 'case AST_CASE: return NULL;',
 'case AST_FOR_IN: return visitor_visit_for_in(visitor,node);']
corps5 = (L(45,70) + L(76,126) + L(129,297) + L(300,430)
      + L(436,470) + L(476,669) + L(672,807))
fin5 = fin[:fin.index("value_t *visitor_new_object")] + "void visitor_free(void){\n}\n"
e5 = ("// Étape 5 du chapitre 7 : structures, énumérations, match et for ... in.\n"
      + entete + corps5 + dispatch(e5cases) + visit + fin5)
open("etape5.c","w").write(e5)
