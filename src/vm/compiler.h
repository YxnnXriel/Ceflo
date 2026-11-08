/**
 * @file compiler.h
 * @brief traduit l'AST d'un fichier en bytecode.
 *
 * [NOTE]
 *      Le compilateur parcourt l'AST une seule fois, comme le faisait le
 *      visitor, mais au lieu d'exécuter chaque noeud il écrit les instructions
 *      qui l'exécuteront. Il résout chaque nom dès la compilation :
 *
 *          variable locale     ->  OP_GET_LOCAL     (une case de la pile)
 *          variable capturée   ->  OP_GET_UPVALUE
 *          attribut, méthode   ->  dans une méthode, `x` veut dire `self.x`
 *          globale du fichier  ->  OP_GET_GLOBAL    (une case du module)
 *          builtin             ->  OP_GET_BUILTIN   (println, input...)
 *
 *      C'est la grande différence avec le visitor, qui cherchait chaque nom
 *      dans une table de hachage à chaque lecture.
 *
 *      Quelques constructions sont traduites en instructions plus simples :
 *      `format("${a}")` avec un texte écrit dans le code devient une suite de
 *      concaténations, compilée dans la portée de l'appel.
 *
 *      Les descriptions de structures (OP_STRUCT) et d'énumérations (OP_ENUM)
 *      sont des constantes : un ObjStruct ou un ObjEnum modèle, avec ses noms
 *      et ses types d'attributs ou ses éléments. L'instruction en crée une
 *      copie à chaque exécution de la définition.
 */

#ifndef __VM_COMPILER_H__
#define __VM_COMPILER_H__

    #include "vm.h"

    // Compile le code d'un fichier. La fonction retournée ne prend aucun
    // argument : l'appeler exécute le fichier et remplit les globales du module.
    // Une erreur de compilation s'affiche et arrête le programme.
    ObjFunction *compile_module(VM *vm, ast_t *program, ObjModule *module);

#endif
