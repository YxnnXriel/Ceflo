#include <math.h>

#include "ceflo.h"

Value mathPi(VM *vm, int argc, Value *args){
    (void)args;
    native_check_args(vm,"mathPi",argc,0);
    return NUMBER_VAL(acos(-1.0));
}

Value mathLog10(VM *vm, int argc, Value *args){
    native_check_args(vm,"mathLog10",argc,1);
    native_check_type(vm,"mathLog10",args[0],"number");
    return NUMBER_VAL(log10(AS_NUMBER(args[0])));
}

Value mathSqrt(VM *vm, int argc, Value *args){
    native_check_args(vm,"mathSqrt",argc,1);
    native_check_type(vm,"mathSqrt",args[0],"number");
    return NUMBER_VAL(sqrt(AS_NUMBER(args[0])));
}
