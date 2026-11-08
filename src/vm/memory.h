/**
 * @file memory.h
 * @brief les allocations et le garbage collector de la VM.
 *
 * [NOTE]
 *      Un simple "mark and sweep" : on marque tout ce qui est atteignable
 *      depuis les racines, puis on libère le reste.
 *
 *      Les racines : la pile de la VM, les closures des CallFrame, les
 *      upvalues ouvertes, les tables de la VM (builtins, modules, noms), et
 *      vm->roots. Les chaînes internées ne sont pas des racines : une chaîne
 *      que plus rien n'utilise quitte la table.
 *
 *      LA RÈGLE QUI REND TOUT SIMPLE : le GC ne se déclenche jamais pendant
 *      une allocation. Il ne tourne qu'aux points sûrs de la VM (gc_safepoint) :
 *      un appel, un retour, le tour d'une boucle. À ces moments-là, toutes les
 *      valeurs vivantes sont sur la pile de la VM.
 *
 *      Le compilateur et les natives peuvent donc allouer autant qu'ils veulent
 *      sans rien protéger. La seule exception : une native qui appelle
 *      vm_call, car le code CeFlo appelé passe par des points sûrs. Elle
 *      protège ses valeurs avec vm_protect (vm.h) ou en les posant sur la pile.
 */

#ifndef __VM_MEMORY_H__
#define __VM_MEMORY_H__

    #include "vm.h"

    #define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

    // realloc qui compte les octets pour le GC. new_size = 0 libère.
    void *vm_reallocate(VM *vm, void *pointer, size_t old_size, size_t new_size);

    #define VM_ALLOCATE(vm,type,count) \
        ((type*)vm_reallocate(vm,NULL,0,sizeof(type) * (count)))
    #define VM_FREE(vm,type,pointer) \
        vm_reallocate(vm,pointer,sizeof(type),0)
    #define VM_GROW_ARRAY(vm,type,pointer,old_count,new_count) \
        ((type*)vm_reallocate(vm,pointer,sizeof(type) * (old_count),sizeof(type) * (new_count)))
    #define VM_FREE_ARRAY(vm,type,pointer,old_count) \
        vm_reallocate(vm,pointer,sizeof(type) * (old_count),0)

    // Alloue un objet et l'ajoute à la liste du GC.
    Obj *vm_allocate_object(VM *vm, size_t size, ObjType type);

    // Lance le GC s'il y a assez de mémoire allouée depuis la dernière fois.
    // Avec CEFLO_GC_STRESS, il tourne à chaque point sûr : c'est `make sanitize`.
    void gc_safepoint(VM *vm);
    void gc_collect(VM *vm);

    void gc_mark_value(VM *vm, Value value);
    void gc_mark_object(VM *vm, Obj *object);
    void gc_mark_table(VM *vm, Table *table);

    // À la fin du programme : libère tous les objets.
    void gc_free_objects(VM *vm);

#endif
