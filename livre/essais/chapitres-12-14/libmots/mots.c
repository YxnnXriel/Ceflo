#include <ctype.h>
#include <stdlib.h>

#include "ceflo.h"

// mots_majuscules("abc") : "ABC"
Value mots_majuscules(VM *vm, int argc, Value *args){
    native_check_args(vm,"mots_majuscules",argc,1);
    native_check_type(vm,"mots_majuscules",args[0],"str");

    ObjString *texte = AS_STRING(args[0]);
    ObjString *resultat = string_reserve(vm,texte->length);
    for(int i = 0; i < texte->length; i++)
        resultat->chars[i] = (char)toupper((unsigned char)texte->chars[i]);
    return OBJ_VAL(resultat);
}

// mots_applique(liste, f) : une nouvelle liste, avec f(x) pour chaque x
Value mots_applique(VM *vm, int argc, Value *args){
    native_check_args(vm,"mots_applique",argc,2);
    native_check_type(vm,"mots_applique",args[0],"list");
    native_check_type(vm,"mots_applique",args[1],"func");

    Value liste = args[0];
    Value fonction = args[1];
    Value resultat = OBJ_VAL(list_new(vm));
    vm_protect(vm,&liste);
    vm_protect(vm,&fonction);
    vm_protect(vm,&resultat);

    for(int i = 0; i < AS_LIST(liste)->count; i++){
        Value x = AS_LIST(liste)->items[i];
        list_append(vm,AS_LIST(resultat),vm_call(vm,fonction,1,&x));
    }

    vm_unprotect(vm,3);
    return resultat;
}
