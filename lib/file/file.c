#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "visitor.h"
#include "error.h"

value_t *file_stdin(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"file_stdin",argc,0);
    return makeGeneric(stdin);
}

value_t *file_stdout(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"file_stdout",argc,0);
    return makeGeneric(stdout);
}

// file_fopen("notes.txt", "r") : le fichier, ou none s'il ne s'ouvre pas
value_t *file_fopen(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"file_fopen",argc,2);
    Value_check_type(node,"file_fopen",args[0],STRING);
    Value_check_type(node,"file_fopen",args[1],STRING);

    FILE *file = fopen(AS_STRING(args[0])->str,AS_STRING(args[1])->str);
    return file != NULL ? makeGeneric(file) : makeNone();
}

value_t *file_fclose(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"file_fclose",argc,1);
    Value_check_type(node,"file_fclose",args[0],GENERIC);

    fclose(args[0]->as.GenericValue);
    return makeNone();
}

// file_fprintf(fichier, valeur, valeur...)
value_t *file_fprintf(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    if(argc < 1)
        visitor_error(node,"`file_fprintf` attend au moins un fichier");
    Value_check_type(node,"file_fprintf",args[0],GENERIC);

    for(int i = 1; i < argc; i++)
        Value_print(visitor,args[0]->as.GenericValue,args[i]);
    return makeNone();
}

// file_fgetline(fichier) : la ligne sans son retour à la ligne, ou none à la fin du fichier
value_t *file_fgetline(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"file_fgetline",argc,1);
    Value_check_type(node,"file_fgetline",args[0],GENERIC);

    FILE *file = args[0]->as.GenericValue;
    int carac = fgetc(file);
    if(carac == EOF)
        return makeNone();

    buffer_t line;
    newBuffer(&line);
    while(carac != EOF && carac != '\n'){
        Buffer_Append_Char(&line,(char)carac);
        carac = fgetc(file);
    }

    value_t *result = makeStringLenght(line.data,(int)line.size);
    Buffer_Free(&line);
    return result;
}
