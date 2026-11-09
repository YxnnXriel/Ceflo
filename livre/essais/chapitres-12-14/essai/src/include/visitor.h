/**
 * @file visitor.h
 * @author Yann Ariel (yannariel13@gmail.com)
 * @brief parcourt l'AST et exécute chaque noeud
 * @copyright Copyright (c) 2023
 */

#ifndef __VISITOR_H__
#define __VISITOR_H__

    #include "ast.h"
    #include "scope.h"
    #include "Value.h"

    // Ce qui interrompt le déroulement normal des instructions
    typedef enum{
        SIGNAL_NONE,
        SIGNAL_RETURN,
        SIGNAL_BREAK,
        SIGNAL_CONTINUE,
    }visitor_signal;

    typedef struct VISITOR_STRUCT{
        scope_t *scope;            // la portée courante
        scope_t *builtins;         // println, input... le parent de chaque fichier
        visitor_signal signal;
        value_t *return_value;
    }visitor_t;

    // Exécute un fichier source avec les arguments donnés au script.
    // Exécute un fichier, puis son `main` s'il en a un. Retourne le code de sortie.
    // Comme en C, argv[0] est le nom du script.
    int visitor_run_file(visitor_t *visitor, int argc, char **argv);
    void visitor_free(void);

    value_t *visitor_visit(visitor_t *visitor, ast_t *node);

    // Appelle une fonction CeFlo ou native. `node` situe les erreurs (peut être NULL).
    value_t *visitor_call(visitor_t *visitor, ast_t *node, value_t *function, value_t **args, int argc);

    // new Nom(args)
    value_t *visitor_new_object(visitor_t *visitor, ast_t *node, value_t *structure, value_t **args, int argc);

#endif
