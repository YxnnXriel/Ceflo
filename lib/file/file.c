#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ceflo.h"
#include "str.h"

// Un fichier ouvert est un `generic` : CeFlo le garde sans savoir ce qu'il contient.
static FILE *file_get(VM *vm, const char *name, Value value){
    native_check_type(vm,name,value,"generic");
    return AS_GENERIC(value)->pointer;
}

Value file_stdin(VM *vm, int argc, Value *args){
    (void)args;
    native_check_args(vm,"file_stdin",argc,0);
    return OBJ_VAL(generic_new(vm,stdin));
}

Value file_stdout(VM *vm, int argc, Value *args){
    (void)args;
    native_check_args(vm,"file_stdout",argc,0);
    return OBJ_VAL(generic_new(vm,stdout));
}

// file_fopen("notes.txt", "r") : le fichier, ou none s'il ne s'ouvre pas
Value file_fopen(VM *vm, int argc, Value *args){
    native_check_args(vm,"file_fopen",argc,2);
    native_check_type(vm,"file_fopen",args[0],"str");
    native_check_type(vm,"file_fopen",args[1],"str");

    FILE *file = fopen(AS_CSTRING(args[0]),AS_CSTRING(args[1]));
    return file != NULL ? OBJ_VAL(generic_new(vm,file)) : NONE_VAL;
}

Value file_fclose(VM *vm, int argc, Value *args){
    native_check_args(vm,"file_fclose",argc,1);
    fclose(file_get(vm,"file_fclose",args[0]));
    return NONE_VAL;
}

// file_fprintf(fichier, valeur, valeur...)
Value file_fprintf(VM *vm, int argc, Value *args){
    if(argc < 1)
        vm_error(vm,"`file_fprintf` attend au moins un fichier");
    FILE *file = file_get(vm,"file_fprintf",args[0]);

    buffer_t buffer;
    newBuffer(&buffer);
    for(int i = 1; i < argc; i++){
        buffer.size = 0;
        vm_value_to_buffer(vm,&buffer,args[i]);
        fwrite(buffer.data,1,buffer.size,file);
    }
    Buffer_Free(&buffer);
    return NONE_VAL;
}

// file_fgetline(fichier) : la ligne sans son retour à la ligne, ou none à la fin du fichier
Value file_fgetline(VM *vm, int argc, Value *args){
    native_check_args(vm,"file_fgetline",argc,1);
    FILE *file = file_get(vm,"file_fgetline",args[0]);

    int carac = fgetc(file);
    if(carac == EOF)
        return NONE_VAL;

    buffer_t line;
    newBuffer(&line);
    while(carac != EOF && carac != '\n'){
        Buffer_Append_Char(&line,(char)carac);
        carac = fgetc(file);
    }

    ObjString *result = string_new(vm,line.data,(int)line.size);
    Buffer_Free(&line);
    return OBJ_VAL(result);
}
