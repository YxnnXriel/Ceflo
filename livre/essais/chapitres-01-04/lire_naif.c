// La lecture qu'on trouve partout : fseek, ftell, un seul fread.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *readFile_naif(const char *path){
    FILE *file = fopen(path,"rb");
    if(file == NULL)
        return NULL;

    fseek(file,0,SEEK_END);
    long size = ftell(file);
    fseek(file,0,SEEK_SET);
    if(size < 0){
        fclose(file);
        return NULL;
    }

    char *code = malloc((size_t)size + 1);
    size_t count = fread(code,1,(size_t)size,file);
    code[count] = '\0';
    fclose(file);
    return code;
}

int main(int argc, char **argv){
    if(argc != 2)
        return 2;
    char *code = readFile_naif(argv[1]);
    if(code == NULL){
        printf("impossible de lire `%s`\n",argv[1]);
        return 1;
    }
    printf("%zu octets lus\n",strlen(code));
    free(code);
    return 0;
}
