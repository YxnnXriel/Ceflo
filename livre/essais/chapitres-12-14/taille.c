#include <stdio.h>
#include <stddef.h>
#include "object.h"
int main(void){
    printf("Obj %zu, Value %zu, ObjInstance %zu (fields a %zu), ObjBoundMethod %zu, ObjStruct %zu, ObjUpvalue %zu, ObjClosure %zu\n",
        sizeof(Obj), sizeof(Value), sizeof(ObjInstance), offsetof(ObjInstance,fields), sizeof(ObjBoundMethod), sizeof(ObjStruct), sizeof(ObjUpvalue), sizeof(ObjClosure));
    return 0;
}
