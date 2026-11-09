// Affiche les jetons d'un fichier CeFlo, un par ligne.
#include <stdio.h>
#include <string.h>

#include "lexer.h"

// La même liste que l'enum, mais avec les noms : #TOKEN fait de TOKEN_ID la chaîne "TOKEN_ID".
#define GENERATE_TOKEN_NAME(TOKEN, TEXT) #TOKEN,
static const char *TOKEN_NAMES[] = {
    FOREACH_TOKEN(GENERATE_TOKEN_NAME)
};

// Affiche le texte d'un jeton, en rendant visibles les \n d'une chaîne.
static void print_value(const token_t *token){
    int lenght = token->type == TOKEN_STRING || token->type == TOKEN_CHAR ?
        token->lenght : (int)strlen(token->value);
    for(int i = 0; i < lenght; i++){
        char c = token->value[i];
        if(c == '\n') printf("\\n");
        else if(c == '\t') printf("\\t");
        else if(c == '\0') printf("\\0");
        else putchar(c);
    }
}

int main(int argc, char **argv){
    if(argc != 2){
        fprintf(stderr,"utilisation : %s fichier.cf\n",argv[0]);
        return 2;
    }

    lexer_t lexer;
    if(!newLexer(&lexer,argv[1])){
        fprintf(stderr,"impossible d'ouvrir `%s`\n",argv[1]);
        return 1;
    }

    while(true){
        token_t token = lexer_get_next_token(&lexer);
        printf("%3d:%-3d %s %-18s ",token.line,token.colone,
            token.newline_before ? "|" : " ",TOKEN_NAMES[token.type]);
        print_value(&token);
        putchar('\n');

        bool end = token.type == TOKEN_EOF;
        freeToken(&token);
        if(end)
            break;
    }
    return 0;
}
