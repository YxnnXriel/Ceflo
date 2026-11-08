/**
 * @file vm.h
 * @brief la machine virtuelle : elle exécute le bytecode.
 *
 * [NOTE]
 *      Une seule pile de valeurs sert à tous les appels. Chaque appel a son
 *      CallFrame : la fonction qui tourne, où elle en est (ip), et où
 *      commencent ses variables dans la pile (slots). La case slots[0] est la
 *      fonction appelée elle-même, ou `self` dans une méthode ; les paramètres
 *      suivent, puis les variables locales.
 *
 *      Il n'y a pas d'exception en CeFlo : une erreur affiche son message et
 *      arrête le programme (vm_error). Une fonction native n'a donc jamais à
 *      vérifier si l'appel qu'elle fait a réussi.
 */

#ifndef __VM_VM_H__
#define __VM_VM_H__

    #include "object.h"

    // Autant d'appels imbriqués que l'ancien interpréteur. La pile est réservée
    // une fois (64 Mo) : le système ne donne la mémoire qu'aux pages touchées.
    #define FRAMES_MAX 100000
    #define STACK_MAX (1 << 22)

    typedef struct{
        ObjClosure *closure;
        uint8_t *ip;
        Value *slots;
    }CallFrame;

    typedef struct VM{
        CallFrame *frames;          // FRAMES_MAX cases, allouées par vm_init
        int frame_count;

        Value *stack;               // STACK_MAX cases
        Value *stack_top;

        ObjUpvalue *open_upvalues;  // les variables capturées encore sur la pile

        Table strings;              // toutes les chaînes internées (le GC les retire)
        Table builtins;             // println, print, input, argv... visibles partout
        Table modules;              // chemin -> ObjModule : un fichier ne s'exécute qu'une fois
        Table native_modules;       // `os`, `console` : nom -> ObjModule déjà rempli

        // Le garbage collector (memory.h)
        Obj *objects;
        size_t bytes_allocated;
        size_t next_gc;
        Obj **gray_stack;
        int gray_count;
        int gray_capacity;
        // Les valeurs qu'une native garde pendant qu'elle rappelle du CeFlo (vm_call).
        Value *roots[64];
        int root_count;

        // Les noms que la VM compare souvent : "__init__", "self", "main"...
        ObjString *name_init;
        ObjString *name_str;
        ObjString *name_cast;
        ObjString *name_main;

        bool disassemble;           // `ceflo --dis` : affiche le bytecode avant de l'exécuter
    }VM;

    void vm_init(VM *vm, int argc, char **argv);
    void vm_free(VM *vm);

    // Exécute un fichier, puis son `main` s'il en a un. Retourne le code de sortie.
    // Comme en C, argv[0] est le nom du script.
    int vm_run_file(VM *vm, const char *path);

    // Appelle une fonction CeFlo ou native depuis du C, et attend son résultat.
    // Les arguments sont copiés : `args` peut être n'importe où.
    Value vm_call(VM *vm, Value function, int argc, Value *args);

    // `new S(args)` et `@S(args)` : crée l'objet, initialise ses attributs, appelle __init__.
    Value vm_new_instance(VM *vm, ObjStruct *structure, int argc, Value *args);

    // Affiche l'erreur à la position de l'instruction en cours et arrête le programme.
    _Noreturn void vm_error(VM *vm, const char *format, ...) __attribute__((format(printf,2,3)));
    // Le noeud de l'AST de l'instruction en cours (NULL hors exécution).
    ast_t *vm_current_node(VM *vm);

    static inline void vm_push(VM *vm, Value value){ *vm->stack_top++ = value; }
    static inline Value vm_pop(VM *vm){ return *--vm->stack_top; }
    static inline Value vm_peek(VM *vm, int distance){ return vm->stack_top[-1 - distance]; }

    // Une native qui appelle vm_call protège ainsi ses valeurs pendant l'appel :
    //      vm_protect(vm,&liste); ... vm_call(...) ...; vm_unprotect(vm,1);
    void vm_protect(VM *vm, Value *value);
    void vm_unprotect(VM *vm, int count);

    // Les règles des opérateurs quand les deux côtés ne sont pas des nombres :
    // "a" + 1, [1] + [2], "ab" * 3, comparaison de textes... (src/value_expr.c)
    Value vm_binary(VM *vm, OpCode op, Value a, Value b);

    // Le texte d'une valeur, tel que println l'affiche. Appelle __str__ si besoin.
    ObjString *vm_to_string(VM *vm, Value value);
    // Ajoute à `buffer` le texte de la valeur, comme vm_to_string, sans créer de chaîne.
    struct STRUCT_BUFFER;
    void vm_value_to_buffer(VM *vm, struct STRUCT_BUFFER *buffer, Value value);

    // Ajoute un builtin : visible dans tous les fichiers.
    void vm_define_builtin(VM *vm, const char *name, NativeFn function);
    void vm_define_builtin_value(VM *vm, const char *name, Value value);

#endif
