/**
 * @file ceflo.h
 * @brief ce qu'il faut inclure pour écrire une bibliothèque native (.so).
 *
 * Une fonction exportée par la bibliothèque a cette forme :
 *
 *      Value ma_fonction(VM *vm, int argc, Value *args){
 *          native_check_args(vm,"ma_fonction",argc,1);
 *          native_check_type(vm,"ma_fonction",args[0],"str");
 *          return NUMBER_VAL(AS_STRING(args[0])->length);
 *      }
 *
 * et s'importe avec `from @"./chemin/libtruc.so" import [ma_fonction]`.
 */

#ifndef __CEFLO_H__
#define __CEFLO_H__

    #include "vm.h"
    #include "natives.h"

    // Arrêtent le programme avec un message clair si l'appel est mauvais.
    void native_check_args(VM *vm, const char *name, int argc, int expected);
    // expected : "none", "bool", "char", "number", "str", "list", "map", "func", "generic"...
    void native_check_type(VM *vm, const char *name, Value value, const char *expected);

#endif
