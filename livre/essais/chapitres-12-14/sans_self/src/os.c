#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "include/os.h"
#include "include/error.h"

/*
    CeFlo n'a pas d'exceptions. Un fichier absent n'est donc pas une erreur qui arrête
    le script : la fonction retourne `none` ou `false`, et le script décide quoi faire.
    Seule une mauvaise utilisation, comme lire(3), arrête le programme.
*/

// Un chemin passe au système comme une chaîne C : elle ne doit pas contenir de `\0`.
static const char *os_text(ast_t *node, const char *name, value_t *value){
    Value_check_type(node,name,value,STRING);
    if(strlen(AS_STRING(value)->str) != (size_t)AS_STRING(value)->lenght)
        visitor_error(node,"`%s` : le texte ne peut pas contenir de caractère nul",name);
    return AS_STRING(value)->str;
}

// Lit tout le flux. Retourne false si la lecture échoue ou si le texte est trop long pour une chaîne.
static bool os_read_stream(FILE *file, buffer_t *buffer){
    char chunk[8192];
    size_t count;

    newBuffer(buffer);
    while((count = fread(chunk,1,sizeof(chunk),file)) > 0){
        if(buffer->size + count > INT_MAX){
            Buffer_Free(buffer);
            return false;
        }
        Buffer_Append(buffer,chunk,count);
    }
    if(ferror(file)){
        Buffer_Free(buffer);
        return false;
    }
    return true;
}

static bool os_read_file(const char *path, buffer_t *buffer){
    FILE *file = fopen(path,"rb");
    if(file == NULL)
        return false;
    bool done = os_read_stream(file,buffer);
    fclose(file);
    return done;
}

// lire("a.txt") : le contenu du fichier, ou none
static value_t *os_lire(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"lire",argc,1);

    buffer_t buffer;
    if(!os_read_file(os_text(node,"lire",args[0]),&buffer))
        return makeNone();

    value_t *text = makeStringLenght(buffer.data,(int)buffer.size);
    Buffer_Free(&buffer);
    return text;
}

// lire_lignes("a.txt") : la liste des lignes, sans leur fin de ligne, ou none
static value_t *os_lire_lignes(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"lire_lignes",argc,1);

    buffer_t buffer;
    if(!os_read_file(os_text(node,"lire_lignes",args[0]),&buffer))
        return makeNone();

    value_t *lines = makeList();
    size_t start = 0;
    while(start < buffer.size){
        char *newline = memchr(buffer.data + start,'\n',buffer.size - start);
        size_t end = newline != NULL ? (size_t)(newline - buffer.data) : buffer.size;
        size_t lenght = end - start;
        if(lenght > 0 && buffer.data[end - 1] == '\r')
            lenght -= 1;
        Value_list_append(lines,makeStringLenght(buffer.data + start,(int)lenght));
        start = end + 1;
    }
    Buffer_Free(&buffer);
    return lines;
}

static value_t *os_write(ast_t *node, const char *name, value_t **args, int argc, const char *mode){
    Value_check_args(node,name,argc,2);
    const char *path = os_text(node,name,args[0]);
    Value_check_type(node,name,args[1],STRING);

    FILE *file = fopen(path,mode);
    if(file == NULL)
        return makeBool(false);

    string_t *text = AS_STRING(args[1]);
    bool done = fwrite(text->str,1,(size_t)text->lenght,file) == (size_t)text->lenght;
    if(fclose(file) != 0)
        done = false;
    return makeBool(done);
}

static value_t *os_ecrire(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    return os_write(node,"ecrire",args,argc,"wb");
}

static value_t *os_ajouter(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    return os_write(node,"ajouter",args,argc,"ab");
}

static value_t *os_existe(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"existe",argc,1);
    struct stat info;
    return makeBool(stat(os_text(node,"existe",args[0]),&info) == 0);
}

static value_t *os_est_dossier(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"est_dossier",argc,1);
    struct stat info;
    return makeBool(stat(os_text(node,"est_dossier",args[0]),&info) == 0 && S_ISDIR(info.st_mode));
}

// taille("a.txt") : le nombre d'octets, ou none
static value_t *os_taille(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"taille",argc,1);
    struct stat info;
    if(stat(os_text(node,"taille",args[0]),&info) != 0)
        return makeNone();
    return makeNumber((double)info.st_size);
}

static int os_compare_names(const void *left, const void *right){
    return strcmp(*(char *const *)left,*(char *const *)right);
}

// dossier(".") : les noms triés, sans `.` ni `..`, ou none
static value_t *os_dossier(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"dossier",argc,1);

    DIR *directory = opendir(os_text(node,"dossier",args[0]));
    if(directory == NULL)
        return makeNone();

    list_t *names = newList();
    struct dirent *entry;
    while((entry = readdir(directory)) != NULL){
        if(strcmp(entry->d_name,".") == 0 || strcmp(entry->d_name,"..") == 0)
            continue;
        char *name = NULL;
        allowChar(&name,entry->d_name);
        List_Append(names,name);
    }
    closedir(directory);

    // readdir donne les noms dans n'importe quel ordre : on veut le même résultat partout.
    qsort(names->Item,(size_t)names->Size,sizeof(void*),os_compare_names);

    value_t *result = makeList();
    for(int i = 0; i < names->Size; i++){
        Value_list_append(result,makeString(List_Get(names,i)));
        free(List_Get(names,i));
    }
    List_Free(names);
    return result;
}

// creer_dossier("a/b/c") crée aussi les dossiers parents, et accepte un dossier qui existe déjà.
static value_t *os_creer_dossier(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"creer_dossier",argc,1);

    const char *text = os_text(node,"creer_dossier",args[0]);
    if(text[0] == '\0')
        return makeBool(false);

    char *path = NULL;
    allowChar(&path,text);

    bool done = true;
    for(char *slash = strchr(path + 1,'/'); done && slash != NULL; slash = strchr(slash + 1,'/')){
        *slash = '\0';
        done = mkdir(path,0777) == 0 || errno == EEXIST;
        *slash = '/';
    }
    if(done)
        done = mkdir(path,0777) == 0 || errno == EEXIST;

    struct stat info;
    done = done && stat(path,&info) == 0 && S_ISDIR(info.st_mode);
    free(path);
    return makeBool(done);
}

// supprimer enlève un fichier ou un dossier vide : jamais un dossier avec son contenu.
static value_t *os_supprimer(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"supprimer",argc,1);
    return makeBool(remove(os_text(node,"supprimer",args[0])) == 0);
}

static value_t *os_renommer(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"renommer",argc,2);
    return makeBool(rename(os_text(node,"renommer",args[0]),os_text(node,"renommer",args[1])) == 0);
}

static value_t *os_copier(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"copier",argc,2);
    const char *target = os_text(node,"copier",args[1]);

    FILE *input = fopen(os_text(node,"copier",args[0]),"rb");
    if(input == NULL)
        return makeBool(false);

    FILE *output = fopen(target,"wb");
    if(output == NULL){
        fclose(input);
        return makeBool(false);
    }

    char chunk[8192];
    size_t count;
    bool done = true;
    while(done && (count = fread(chunk,1,sizeof(chunk),input)) > 0)
        done = fwrite(chunk,1,count,output) == count;

    done = done && ferror(input) == 0;
    fclose(input);
    if(fclose(output) != 0)
        done = false;
    return makeBool(done);
}

// chemin("a", "b", "c.txt") donne "a/b/c.txt", sans doubler les `/`
static value_t *os_chemin(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    buffer_t buffer;
    newBuffer(&buffer);

    for(int i = 0; i < argc; i++){
        Value_check_type(node,"chemin",args[i],STRING);
        string_t *part = AS_STRING(args[i]);
        if(part->lenght == 0)
            continue;
        if(buffer.size > 0 && buffer.data[buffer.size - 1] != '/' && part->str[0] != '/')
            Buffer_Append_Char(&buffer,'/');
        if(buffer.size > 0 && buffer.data[buffer.size - 1] == '/' && part->str[0] == '/')
            Buffer_Append(&buffer,part->str + 1,(size_t)part->lenght - 1);
        else
            Buffer_Append(&buffer,part->str,(size_t)part->lenght);
    }

    value_t *result = makeStringLenght(buffer.data,(int)buffer.size);
    Buffer_Free(&buffer);
    return result;
}

static value_t *os_dossier_courant(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"dossier_courant",argc,0);

    char *path = getcwd(NULL,0);
    if(path == NULL)
        return makeNone();
    value_t *result = makeString(path);
    free(path);
    return result;
}

static value_t *os_changer_dossier(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"changer_dossier",argc,1);
    return makeBool(chdir(os_text(node,"changer_dossier",args[0])) == 0);
}

// env("HOME") : la valeur de la variable d'environnement, ou none
static value_t *os_env(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"env",argc,1);
    const char *value = getenv(os_text(node,"env",args[0]));
    return value != NULL ? makeString(value) : makeNone();
}

// Un programme arrêté par un signal donne 128 + le signal, comme dans le shell.
static int os_exit_code(int status){
    if(status == -1)
        return -1;
    if(WIFEXITED(status))
        return WEXITSTATUS(status);
    return WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1;
}

// commande("ls") : { code: 0, sortie: "..." }. Le texte passe par le shell.
static value_t *os_commande(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"commande",argc,1);
    const char *command = os_text(node,"commande",args[0]);

    // Ce qui attend d'être affiché doit sortir avant le texte de la commande.
    fflush(stdout);
    FILE *pipe = popen(command,"r");
    if(pipe == NULL)
        return makeNone();

    buffer_t buffer;
    bool done = os_read_stream(pipe,&buffer);
    int code = os_exit_code(pclose(pipe));
    if(!done)
        return makeNone();

    value_t *result = makeMap();
    Value_map_set_name(result,"code",makeNumber(code));
    Value_map_set_name(result,"sortie",makeStringLenght(buffer.data,(int)buffer.size));
    Buffer_Free(&buffer);
    return result;
}

// lancer("make") : la commande affiche directement son texte, et on reçoit son code de sortie.
static value_t *os_lancer(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"lancer",argc,1);
    const char *command = os_text(node,"lancer",args[0]);
    fflush(stdout);
    return makeNumber(os_exit_code(system(command)));
}

typedef struct{
    const char *name;
    ceflo_func_t func;
}os_function;

static const os_function Os_List[] = {
    {"lire",&os_lire},
    {"lire_lignes",&os_lire_lignes},
    {"ecrire",&os_ecrire},
    {"ajouter",&os_ajouter},
    {"existe",&os_existe},
    {"est_dossier",&os_est_dossier},
    {"taille",&os_taille},
    {"dossier",&os_dossier},
    {"creer_dossier",&os_creer_dossier},
    {"supprimer",&os_supprimer},
    {"renommer",&os_renommer},
    {"copier",&os_copier},
    {"chemin",&os_chemin},
    {"dossier_courant",&os_dossier_courant},
    {"changer_dossier",&os_changer_dossier},
    {"env",&os_env},
    {"commande",&os_commande},
    {"lancer",&os_lancer},
    {NULL,NULL}
};

void os_define_all(scope_t *scope){
    for(const os_function *item = Os_List; item->name != NULL; item++)
        scope_define(scope,item->name,makeBuiltin(item->name,item->func));
}
