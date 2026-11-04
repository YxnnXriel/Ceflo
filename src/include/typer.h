#ifndef __TYPER_H__
#define __TYPER_H__

    #include "visitor.h"

    // Vrai si la valeur est bien de ce type (`int`, `str`, le nom d'une structure...).
    // `::type` ne convertit jamais : seul @type(valeur) le fait.
    bool typer_has_type(visitor_t *visitor, ast_t *node, value_t *value, const char *type_name);

    // Le type d'une valeur dans un message d'erreur : `3` est un int et `3.5` un float.
    const char *typer_type_name(value_t *value);

    // La valeur d'une variable déclarée sans `=` : `var a::int` vaut 0.
    value_t *typer_default_value(visitor_t *visitor, ast_t *node, const char *type_name);

    // @type(args)
    value_t *typer_typer(visitor_t *visitor, ast_t *node, value_t **args, int argc);

#endif
