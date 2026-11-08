#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "ceflo.h"
#include "str.h"

// Écrit le texte d'une valeur, comme println. Une chaîne s'écrit sans copie.
static void core_write(VM *vm, FILE *file, Value value){
    if(IS_STRING(value)){
        fwrite(AS_CSTRING(value),1,(size_t)AS_STRING(value)->length,file);
        return;
    }

    buffer_t buffer;
    newBuffer(&buffer);
    vm_value_to_buffer(vm,&buffer,value);
    fwrite(buffer.data,1,buffer.size,file);
    Buffer_Free(&buffer);
}

static Value core_print(VM *vm, int argc, Value *args){
    for(int i = 0; i < argc; i++)
        core_write(vm,stdout,args[i]);
    return NONE_VAL;
}

static Value core_println(VM *vm, int argc, Value *args){
    core_print(vm,argc,args);
    fputc('\n',stdout);
    return NONE_VAL;
}

// Lit une ligne sans sa fin de ligne. Retourne false à la fin de l'entrée.
static bool core_read_line(buffer_t *line){
    int carac = fgetc(stdin);
    if(carac == EOF)
        return false;

    while(carac != EOF && carac != '\n'){
        Buffer_Append_Char(line,(char)carac);
        carac = fgetc(stdin);
    }

    if(line->size > 0 && line->data[line->size - 1] == '\r'){
        line->size -= 1;
        line->data[line->size] = '\0';
    }
    return true;
}

// input("prompt") : la ligne lue, ou none à la fin de l'entrée
static Value core_input(VM *vm, int argc, Value *args){
    core_print(vm,argc,args);
    fflush(stdout);

    buffer_t line;
    newBuffer(&line);

    Value result = core_read_line(&line) ?
        OBJ_VAL(string_new(vm,line.data,(int)line.size)) : NONE_VAL;

    Buffer_Free(&line);
    return result;
}

static Value core_nextInt(VM *vm, int argc, Value *args){
    core_print(vm,argc,args);
    fflush(stdout);

    buffer_t line;
    newBuffer(&line);

    char *end = NULL;
    double number = 0;
    bool valid = core_read_line(&line);
    if(valid){
        number = strtod(line.data,&end);
        valid = end != line.data && *end == '\0';
    }
    Buffer_Free(&line);

    if(!valid)
        vm_error(vm,"`nextInt` : un nombre était attendu");

    return NUMBER_VAL(number);
}

static Value core_exit(VM *vm, int argc, Value *args){
    if(argc == 0)
        exit(0);

    native_check_args(vm,"exit",argc,1);
    native_check_type(vm,"exit",args[0],"number");

    // Comme un char : exit(256) vaut 0 et exit(-1) vaut 255.
    double number = AS_NUMBER(args[0]);
    char code = isfinite(number) ? (char)(int)fmod(number,256) : '\0';
    exit((int)code & 0xFF);
}


//                          ===================================== $FORMAT =================================================


/*
    format("${a}") avec un texte écrit dans le code est compilé (compiler.h) :
    ${a} y voit les variables locales. Cette native ne reçoit donc qu'un texte
    calculé pendant l'exécution, où plus aucun nom n'est connu.
*/
static ObjString *core_format_text(VM *vm, const char *name, int argc, Value *args){
    native_check_args(vm,name,argc,1);
    native_check_type(vm,name,args[0],"str");

    ObjString *format = AS_STRING(args[0]);
    if(memchr(format->chars,'$',(size_t)format->length) == NULL)
        return format;

    buffer_t out;
    newBuffer(&out);

    for(int i = 0; i < format->length; i++){
        char carac = format->chars[i];

        if(carac == '\\' && i + 1 < format->length && format->chars[i + 1] == '$'){
            Buffer_Append_Char(&out,'$');
            i += 1;
        }else if(carac == '$' && i + 1 < format->length && format->chars[i + 1] == '{'){
            Buffer_Free(&out);
            vm_error(vm,"`%s` : le texte doit être écrit dans le code pour utiliser ${...}",name);
        }else{
            Buffer_Append_Char(&out,carac);
        }
    }

    ObjString *result = string_new(vm,out.data,(int)out.size);
    Buffer_Free(&out);
    return result;
}

static Value core_format(VM *vm, int argc, Value *args){
    return OBJ_VAL(core_format_text(vm,"format",argc,args));
}

static Value core_printf(VM *vm, int argc, Value *args){
    core_write(vm,stdout,OBJ_VAL(core_format_text(vm,"printf",argc,args)));
    return NONE_VAL;
}


//                          ===================================== $TABLE ==================================================


typedef struct{
    const char *name;
    NativeFn function;
}core_function;

static const core_function Core_List[] = {
    {"println",&core_println},
    {"print",&core_print},
    {"input",&core_input},
    {"format",&core_format},
    {"printf",&core_printf},
    {"nextInt",&core_nextInt},
    {"exit",&core_exit},
    {NULL,NULL}
};

void natives_define_core(VM *vm, int argc, char **argv){
    for(const core_function *item = Core_List; item->name != NULL; item++)
        vm_define_builtin(vm,item->name,item->function);

    // Le GC ne tourne jamais pendant une allocation : la liste n'a pas besoin d'être protégée.
    ObjList *arguments = list_new(vm);
    for(int i = 0; i < argc; i++)
        list_append(vm,arguments,OBJ_VAL(string_new(vm,argv[i],(int)strlen(argv[i]))));
    vm_define_builtin_value(vm,"argv",OBJ_VAL(arguments));
}
