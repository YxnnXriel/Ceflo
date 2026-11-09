/**
 * @file natives.h
 * @brief tout ce qui est écrit en C et visible depuis CeFlo : les builtins,
 *        les méthodes de list, map, str et enum, les conversions @type(),
 *        les modules `os` et `console`, et les bibliothèques .so.
 */

#ifndef __VM_NATIVES_H__
#define __VM_NATIVES_H__

    #include "vm.h"

    // println, print, input, nextInt, exit, format, printf et argv.
    void natives_define_core(VM *vm, int argc, char **argv);

    // Les modules compilés dans l'interpréteur : `from os import [lire]`.
    // Retourne NULL si `name` (os, console...) n'en est pas un.
    ObjModule *natives_standard_module(VM *vm, const char *name);

    // `liste.append(x)`, `texte.split(",")`, `Couleur.values()`...
    // `found` reste false si ce type n'a pas cette méthode.
    Value natives_call_method(VM *vm, Value receiver, ObjString *name, int argc, Value *args, bool *found);

    // Les types écrits avec `::`. TYPE_NAMED est cherché dans les globales de `module`.
    bool typer_check(VM *vm, ObjModule *module, Value value, TypeRef *type);
    // La valeur d'une variable déclarée sans `=` : `var a::int` vaut 0.
    Value typer_default(VM *vm, ObjModule *module, TypeRef *type);
    // `@int(x)`, `@str(x)`, `@Point(1, 2)`, `@Couleur("ROUGE")`...
    Value typer_type_call(VM *vm, ObjModule *module, ObjString *type_name, int argc, Value *args);
    // Le TypeKind d'un nom de type écrit dans le code ("int", "str", "Point"...).
    TypeKind typer_kind(const char *type_name);

    // `from @"lib/file/libfile.so" import [file_fopen]` : la fonction C de ce nom.
    Value natives_load_library(VM *vm, const char *path, ObjString *name);
    // Ferme les bibliothèques à la fin du programme.
    void natives_free(VM *vm);

#endif
