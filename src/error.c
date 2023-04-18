#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include "include/error.h"


void Print_parser_error(int line , int colone ,char *fileName, errorE err , char *fmt , va_list args ){


    printf("%s:%d:%d: %s: ",fileName,line,colone,ERROR_TO_STRING[err]);
    
    char buffer;
    vprintf(
        fmt,
        args
    );
    va_end(args);

}

void Print_error(lexer_t * lexer , char *fmt, ...){
    printf("%s:%d:%d: ","./Ceflo",lexer->line,lexer->colone);
    va_list args;
    va_start(args,fmt);
    vprintf(fmt,args);
    va_end(args);

}

static void visitor_print_six(int fmt,int line){
    if(line == -1){
    
        printf("   ");
        for (int i = 0; i < fmt; i++){
            printf(" ");
        }
        printf("   ");
        printf("|");
    
    }else{
        printf("   ");
        printf("%d",line);
        printf("   ");
        printf("|");
    }
}

void Print_visitor_error(int line, int colone,char *file_name,char *format,...){

    char buffer[BUFSIZ];
    va_list args;
    va_start(args,format);
    vsnprintf(buffer,BUFSIZ-1,format,args);
    
    printf("\n");

    int fmt = (log10(line) +1 ) ;
    for (int i = 0; i < 5; i++){
        if(i == 3){
            visitor_print_six(fmt,line);
            printf("    %s",buffer);
        }if(i == 0){
            visitor_print_six(fmt,-1);
            printf("FICHIER -> %s[%d:%d]\n",file_name ? file_name:".vil",line,colone);        
        }else{
            visitor_print_six(fmt,-1);
            printf("\n");
        }
    }
       
    exit(1);
}