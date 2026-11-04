/**
 * @file ast.h
 * @author Yann Ariel (yannariel13@gmail.com)
 * @brief l'arbre produit par le parser. Il décrit le programme et ne change
 *        jamais pendant l'exécution : les valeurs vivent dans Value.h.
 * @copyright Copyright (c) 2023
 */

#ifndef __AST_H__
#define __AST_H__

    #include "common.h"
    #include "list.h"
    #include "token.h"

    typedef enum{
        AST_NOOP,
        AST_COMPOUND,            // { instructions... }

        // Valeurs écrites dans le code
        AST_NONE,
        AST_BOOL,
        AST_CHAR,
        AST_NUMBER,
        AST_STRING,
        AST_LIST,
        AST_MAP,                 // { a: 1 } : list_compound contient clé, valeur, clé, valeur...
        AST_LFUNC,

        // Expressions
        AST_VARIABLE,
        AST_OBJ_ACCESS,          // a.b
        AST_LIST_ACCESS,         // a[b]
        AST_FUNCTION_CALL,       // a(b)
        AST_STRUCT,              // new A(b)
        AST_TYPE_CALL,           // @A(b)
        AST_BINOP,
        AST_UNOP,
        AST_TERNARY,

        // Instructions
        AST_VARIABLE_DEFINITION,
        AST_FUNCTION_DEFINITION,
        AST_STRUCT_DEFINITION,
        AST_ASSIGNEMENT,
        AST_RETURN,
        AST_BREAK,
        AST_CONTINUE,
        AST_IF,
        AST_WHILE,
        AST_FOR,
        AST_FOR_IN,              // for var a, b in c : for_init, for_condition (c), for_body
        AST_INCLUDE,
        AST_EXPORT,
    }ast_type;

    typedef struct AST_STRUCT{

        ast_type type;

        /* ======  position, pour les erreurs ====== */
            const char *file;
            int line;
            int colone;

        /* ======  AST_VARIABLE_DEFINITION ====== */
            char *variable_definition_name;
            struct AST_STRUCT *variable_definition_value;
            char *variable_definition_static_type_name;
            bool is_const;

        /* ======  AST_VARIABLE, AST_OBJ_ACCESS ====== */
            char *variable_name;
            uint32_t variable_hash;     // AST_VARIABLE : calculé à la première lecture

        /* ======  AST_FUNCTION_DEFINITION, AST_LFUNC ====== */
            char *function_definition_name;
            list_t *function_definition_args;
            struct AST_STRUCT *function_definition_body;
            char *function_definition_return_type_name;
            bool is_define_self;

        /* ======  AST_FUNCTION_CALL, AST_STRUCT, AST_TYPE_CALL ====== */
            char *function_call_name;
            list_t *function_call_args;

        /* ======  valeurs écrites dans le code ====== */
            double number_value;
            bool bool_value;
            char char_value;
            char *string_value;
            int string_lenght;

        /* ======  binop, unop, accès, appel ====== */
            struct AST_STRUCT *left;
            struct AST_STRUCT *right;
            TokenEnum op;

        /* ======  return ====== */
            struct AST_STRUCT *return_expr;

        /* ======  if, ternary ====== */
            struct AST_STRUCT *if_expr;
            struct AST_STRUCT *if_body;
            struct AST_STRUCT *if_other;
            struct AST_STRUCT *else_body;

        /* ======  while ====== */
            struct AST_STRUCT *while_expr;
            struct AST_STRUCT *while_body;

        /* ======  for ====== */
            struct AST_STRUCT *for_init;
            struct AST_STRUCT *for_condition;
            struct AST_STRUCT *for_solve;
            struct AST_STRUCT *for_body;

        /* ======  assignement ====== */
            struct AST_STRUCT *assignement_left;
            struct AST_STRUCT *assignement_value;

        /* ======  list_access ====== */
            struct AST_STRUCT *list_ptr;

        /* ======  struct definition ====== */
            char *struct_definition_name;
            list_t *struct_definition_item;

        /* ======  include ====== */
            bool is_dll;
            bool is_import_all;

        /* ======  compound, list, include, export ====== */
            list_t *list_compound;
            bool has_definition;   // le bloc déclare un nom : il lui faut sa propre porté

        /* ======  tous les noeuds, pour les libérer à la fin ====== */
            struct AST_STRUCT *next;

    }ast_t;

    ast_t *newAst(ast_type type);
    ast_t *newAstWithLine(ast_type type, const char *file, int line, int colone);
    void clearAst(void);

    // Retrouve un élément d'une structure. Retourne NULL s'il n'existe pas.
    ast_t *GetFunctionInStruct(ast_t *structure, const char *name);

#endif
