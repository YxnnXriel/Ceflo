/**
 * @file module.c
 * @brief exécuter un fichier, son `main`, et les imports entre fichiers.
 *
 * [NOTE]
 *      Un fichier ne s'exécute qu'une seule fois : vm->modules garde chaque
 *      module chargé, sous son chemin. Le module est rangé AVANT d'exécuter
 *      son code : deux fichiers qui s'importent l'un l'autre ne bouclent pas.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "vm.h"
#include "memory.h"
#include "compiler.h"
#include "natives.h"
#include "parser.h"
#include "error.h"

// Le chemin écrit par le dernier `import` : les erreurs d'OP_IMPORT_NAME le citent.
static ObjString *last_import_path;

// `--dis` : la fonction, puis chaque fonction rangée dans ses constantes.
static void disassemble_function(ObjFunction *function){
    chunk_disassemble(&function->chunk,function->name != NULL ? function->name->chars : function->module->path->chars);
    for(int i = 0; i < function->chunk.constants.count; i++){
        Value constant = function->chunk.constants.values[i];
        if(IS_FUNCTION(constant))
            disassemble_function(AS_FUNCTION(constant));
    }
}

// Analyse, compile et exécute un fichier. Le module est rangé avant l'exécution.
static ObjModule *run_source(VM *vm, lexer_t *lexer){
    ast_t *program = parser_parse(lexer);

    ObjModule *module = module_new(vm,string_from(vm,lexer->fileName));
    table_set(&vm->modules,module->path,OBJ_VAL(module));

    ObjFunction *function = compile_module(vm,program,module);
    if(vm->disassemble)
        disassemble_function(function);

    Value closure = OBJ_VAL(closure_new(vm,function));
    vm_call(vm,closure,0,NULL);
    module->loaded = true;
    return module;
}

// Un chemin se cherche à côté du fichier qui importe, puis depuis le dossier courant.
static ObjString *resolve_path(VM *vm, ObjString *path){
    ast_t *node = vm_current_node(vm);
    const char *slash = node != NULL ? strrchr(node->file,'/') : NULL;

    if(path->chars[0] != '/' && slash != NULL){
        size_t dir = (size_t)(slash - node->file) + 1;
        char *full = malloc(dir + (size_t)path->length + 1);
        memcpy(full,node->file,dir);
        memcpy(full + dir,path->chars,(size_t)path->length + 1);

        FILE *file = fopen(full,"rb");
        if(file != NULL){
            fclose(file);
            ObjString *result = string_from(vm,full);
            free(full);
            return result;
        }
        free(full);
    }
    return path;
}

Value module_import(VM *vm, ObjString *path){
    ObjString *full = resolve_path(vm,path);
    Value loaded;
    ObjModule *module = NULL;

    if(table_get(&vm->modules,full,&loaded)){
        module = AS_MODULE(loaded);
    }else{
        lexer_t lexer;
        if(newLexer(&lexer,full->chars))
            module = run_source(vm,&lexer);
        else
            module = natives_standard_module(vm,path->chars);
        if(module == NULL)
            vm_error(vm,"impossible d'ouvrir `%s`",path->chars);
    }

    last_import_path = path;
    return OBJ_VAL(module);
}

static bool is_exported(ObjModule *module, ObjString *name){
    if(module->exports == NULL)
        return true;
    for(int i = 0; i < module->export_count; i++){
        if(module->exports[i] == name)
            return true;
    }
    return false;
}

// La globale `from->globals[index]` devient aussi une globale de `into`, avec son type et sa constance.
static void copy_global(VM *vm, ObjModule *into, ObjModule *from, int index){
    int target = module_global_index(vm,into,from->names[index]);
    into->globals.values[target] = from->globals.values[index];
    into->types[target] = from->types[index];
    into->is_const[target] = from->is_const[index];
}

// La case de `name` dans `from`, après avoir vérifié qu'il existe et qu'il est exporté.
static int find_export(VM *vm, ObjModule *from, ObjString *name){
    const char *path = last_import_path != NULL ? last_import_path->chars : from->path->chars;
    Value index;
    if(!table_get(&from->indexes,name,&index) || IS_UNDEFINED(from->globals.values[(int)AS_NUMBER(index)]))
        vm_error(vm,"`%s` n'existe pas dans `%s`",name->chars,path);
    if(!is_exported(from,name))
        vm_error(vm,"`%s` n'est pas exporté par `%s`",name->chars,path);
    return (int)AS_NUMBER(index);
}

void module_import_name(VM *vm, ObjModule *into, ObjModule *from, ObjString *name){
    copy_global(vm,into,from,find_export(vm,from,name));
}

Value module_import_value(VM *vm, ObjModule *from, ObjString *name){
    return from->globals.values[find_export(vm,from,name)];
}

// Le `main` d'un module n'empêche pas le fichier qui l'importe d'avoir le sien.
void module_import_all(VM *vm, ObjModule *into, ObjModule *from){
    for(int i = 0; i < from->globals.count; i++){
        ObjString *name = from->names[i];
        if(IS_UNDEFINED(from->globals.values[i]) || name == vm->name_main || !is_exported(from,name))
            continue;
        copy_global(vm,into,from,i);
    }
}

// export :: [a, b] : le fichier ne publie plus que ces noms.
void module_export(VM *vm, ObjModule *module, ObjString *name){
    Value index;
    if(!table_get(&module->indexes,name,&index) || IS_UNDEFINED(module->globals.values[(int)AS_NUMBER(index)]))
        vm_error(vm,"`%s` n'existe pas : on ne peut pas l'exporter",name->chars);

    module->exports = VM_GROW_ARRAY(vm,ObjString*,module->exports,module->export_count,module->export_count + 1);
    module->exports[module->export_count++] = name;
}

/*
    Appelle le `main` écrit dans ce fichier, une fois le haut du fichier
    exécuté. Le `main` d'un module importé n'est pas lancé. Retourne le code
    de sortie.
*/
static int run_main(VM *vm, ObjModule *module){
    Value index;
    if(!table_get(&module->indexes,vm->name_main,&index))
        return 0;

    Value function = module->globals.values[(int)AS_NUMBER(index)];
    if(!IS_CLOSURE(function) || AS_CLOSURE(function)->function->module != module)
        return 0;

    ast_t *definition = AS_CLOSURE(function)->function->definition;
    int argc = AS_CLOSURE(function)->function->arity;
    if(argc > 1)
        visitor_error(definition,"`main` prend au plus un paramètre : argv");

    Value arguments = NONE_VAL;
    table_get(&vm->builtins,string_from(vm,"argv"),&arguments);
    Value result = vm_call(vm,function,argc,&arguments);

    if(IS_NONE(result))
        return 0;
    if(!IS_NUMBER(result) || AS_NUMBER(result) != trunc(AS_NUMBER(result)))
        visitor_error(definition,"`main` doit retourner un int, pas %s",value_typer_name(result));

    double code = AS_NUMBER(result);
    return (isfinite(code) ? (char)(int)fmod(code,256) : 0) & 0xFF;
}

int vm_run_file(VM *vm, const char *path){
    lexer_t lexer;
    if(!newLexer(&lexer,path)){
        fprintf(stderr,"erreur: impossible d'ouvrir `%s`\n",path);
        exit(1);
    }
    return run_main(vm,run_source(vm,&lexer));
}
