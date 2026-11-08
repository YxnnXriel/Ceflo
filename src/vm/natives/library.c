#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
    #include <dlfcn.h>
#endif

#include "ceflo.h"
#include "str.h"


//                          ===================================== $VERIFICATIONS ==========================================


void native_check_args(VM *vm, const char *name, int argc, int expected){
    if(argc != expected)
        vm_error(vm,"`%s` prend %d argument(s), mais en reçoit %d",name,expected,argc);
}

// Le nom de la famille d'une valeur, comme l'écrit une bibliothèque : "number", "func"...
static const char *native_kind(Value value){
    switch(value.type){
        case VAL_NONE: return "none";
        case VAL_BOOL: return "bool";
        case VAL_CHAR: return "char";
        case VAL_NUMBER: return "number";
        case VAL_UNDEFINED: return "none";
        case VAL_OBJ: break;
    }
    switch(OBJ_TYPE(value)){
        case OBJ_STRING: return "str";
        case OBJ_LIST: return "list";
        case OBJ_MAP: return "map";
        case OBJ_INSTANCE: return "object";
        case OBJ_STRUCT: return "struct";
        case OBJ_ENUM:
        case OBJ_ENUM_VALUE: return "enum";
        case OBJ_GENERIC: return "generic";
        case OBJ_MODULE: return "module";
        default: return "func";
    }
}

void native_check_type(VM *vm, const char *name, Value value, const char *expected){
    if(strcmp(native_kind(value),expected) != 0)
        vm_error(vm,"`%s` attend un argument de type %s, pas %s",name,expected,value_type_name(value));
}


//                          ===================================== $BIBLIOTHEQUES ==========================================


#ifndef _WIN32

// Les bibliothèques déjà ouvertes : un même .so n'est ouvert qu'une fois.
typedef struct{
    char *path;
    void *handle;
}library_t;

static library_t *libraries = NULL;
static int library_count = 0;

// Le nom d'une ObjNative doit vivre jusqu'à la fin : name->chars peut être libéré par le GC.
static char **native_names = NULL;
static int native_name_count = 0;

// Comme pour un import : le chemin est d'abord cherché à côté du fichier qui l'importe.
static char *library_resolve_path(VM *vm, const char *path){
    ast_t *node = vm_current_node(vm);
    const char *slash = node != NULL && node->file != NULL ? strrchr(node->file,'/') : NULL;

    if(path[0] != '/' && slash != NULL){
        buffer_t full;
        newBuffer(&full);
        Buffer_Append(&full,node->file,(size_t)(slash - node->file) + 1);
        Buffer_Append_Str(&full,path);

        FILE *file = fopen(full.data,"rb");
        if(file != NULL){
            fclose(file);
            return full.data;
        }
        Buffer_Free(&full);
    }
    return strdup(path);
}

static void *library_open(VM *vm, const char *written){
    char *path = library_resolve_path(vm,written);

    for(int i = 0; i < library_count; i++){
        if(strcmp(libraries[i].path,path) == 0){
            free(path);
            return libraries[i].handle;
        }
    }

    void *handle = dlopen(path,RTLD_LAZY);
    if(handle == NULL){
        free(path);
        vm_error(vm,"impossible d'ouvrir `%s` : %s",written,dlerror());
    }

    libraries = realloc(libraries,sizeof(library_t) * (size_t)(library_count + 1));
    libraries[library_count].path = path;
    libraries[library_count].handle = handle;
    library_count += 1;
    return handle;
}

Value natives_load_library(VM *vm, const char *path, ObjString *name){
    void *handle = library_open(vm,path);

    // dlsym retourne un void* : la copie évite une conversion interdite en C.
    void *symbol = dlsym(handle,name->chars);
    if(symbol == NULL)
        vm_error(vm,"`%s` n'existe pas dans `%s`",name->chars,path);

    NativeFn function = NULL;
    memcpy(&function,&symbol,sizeof(function));
    native_names = realloc(native_names,sizeof(char*) * (size_t)(native_name_count + 1));
    native_names[native_name_count] = strdup(name->chars);
    return OBJ_VAL(native_new(vm,native_names[native_name_count++],function));
}

void natives_free(VM *vm){
    (void)vm;
    for(int i = 0; i < library_count; i++){
        dlclose(libraries[i].handle);
        free(libraries[i].path);
    }
    free(libraries);
    libraries = NULL;
    library_count = 0;

    for(int i = 0; i < native_name_count; i++)
        free(native_names[i]);
    free(native_names);
    native_names = NULL;
    native_name_count = 0;
}

#else

Value natives_load_library(VM *vm, const char *path, ObjString *name){
    (void)path;(void)name;
    vm_error(vm,"les bibliothèques natives ne sont pas encore disponibles sous Windows");
}

void natives_free(VM *vm){
    (void)vm;
}

#endif
