#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "include/error.h"
#include "include/ast.h"

typedef struct SOURCE_STRUCT{
    char *name;
    char *code;
    struct SOURCE_STRUCT *next;
}source_t;

static source_t *sources;

const char *error_add_source(const char *name, char *code){
    source_t *source = ceflo_alloc(sizeof(*source));
    allowChar(&source->name,name);
    source->code = code;
    source->next = sources;
    sources = source;
    return source->name;
}

void error_free_sources(void){
    while(sources != NULL){
        source_t *next = sources->next;
        free(sources->name);
        free(sources->code);
        free(sources);
        sources = next;
    }
}

static const char *error_find_line(const char *file, int line){
    for(source_t *source = sources; source != NULL; source = source->next){
        if(source->name != file)
            continue;

        const char *cursor = source->code;
        for(int i = 1; i < line; i++){
            cursor = strchr(cursor,'\n');
            if(cursor == NULL)
                return NULL;
            cursor += 1;
        }
        return cursor;
    }
    return NULL;
}

/**
 * @brief affiche la ligne fautive, et un `^` sous la colonne de l'erreur
 */
static void error_print_location(const char *file, int line, int colone){
    if(file == NULL)
        return;

    fprintf(stderr,"  --> %s:%d:%d\n",file,line,colone);

    const char *text = error_find_line(file,line);
    if(text == NULL)
        return;

    int lenght = (int)strcspn(text,"\r\n");
    fprintf(stderr,"%5d | %.*s\n",line,lenght,text);
    fprintf(stderr,"      | ");
    for(int i = 0; i < colone - 1 && i < lenght; i++){
        // Les octets de suite UTF-8 n'occupent pas de colonne à l'écran.
        if(((unsigned char)text[i] & 0xC0) != 0x80)
            fputc(text[i] == '\t' ? '\t' : ' ',stderr);
    }
    fprintf(stderr,"^\n");
}

static _Noreturn void error_report(const char *title, const char *file, int line, int colone, const char *format, va_list args){
    fflush(stdout);
    fprintf(stderr,"%s: ",title);
    vfprintf(stderr,format,args);
    fprintf(stderr,"\n");
    error_print_location(file,line,colone);
    exit(1);
}

void error_syntax(const char *file, int line, int colone, const char *format, ...){
    va_list args;
    va_start(args,format);
    error_report("erreur de syntaxe",file,line,colone,format,args);
}

void visitor_error(const ast_t *node, const char *format, ...){
    va_list args;
    va_start(args,format);
    error_report(
        "erreur",
        node != NULL ? node->file : NULL,
        node != NULL ? node->line : 0,
        node != NULL ? node->colone : 0,
        format,
        args
    );
}
