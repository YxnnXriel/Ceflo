#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "ceflo.h"
#include "str.h"

/*
    CeFlo n'a pas d'exceptions. Un fichier absent n'est donc pas une erreur qui arrête
    le script : la fonction retourne `none` ou `false`, et le script décide quoi faire.
    Seule une mauvaise utilisation, comme lire(3), arrête le programme.
*/

// Un chemin passe au système comme une chaîne C : elle ne doit pas contenir de `\0`.
static const char *os_text(VM *vm, const char *name, Value value){
    native_check_type(vm,name,value,"str");
    if(strlen(AS_CSTRING(value)) != (size_t)AS_STRING(value)->length)
        vm_error(vm,"`%s` : le texte ne peut pas contenir de caractère nul",name);
    return AS_CSTRING(value);
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
static Value os_lire(VM *vm, int argc, Value *args){
    native_check_args(vm,"lire",argc,1);

    buffer_t buffer;
    if(!os_read_file(os_text(vm,"lire",args[0]),&buffer))
        return NONE_VAL;

    ObjString *text = string_new(vm,buffer.data,(int)buffer.size);
    Buffer_Free(&buffer);
    return OBJ_VAL(text);
}

// lire_lignes("a.txt") : la liste des lignes, sans leur fin de ligne, ou none
static Value os_lire_lignes(VM *vm, int argc, Value *args){
    native_check_args(vm,"lire_lignes",argc,1);

    buffer_t buffer;
    if(!os_read_file(os_text(vm,"lire_lignes",args[0]),&buffer))
        return NONE_VAL;

    ObjList *lines = list_new(vm);
    size_t start = 0;
    while(start < buffer.size){
        char *newline = memchr(buffer.data + start,'\n',buffer.size - start);
        size_t end = newline != NULL ? (size_t)(newline - buffer.data) : buffer.size;
        size_t lenght = end - start;
        if(lenght > 0 && buffer.data[end - 1] == '\r')
            lenght -= 1;
        list_append(vm,lines,OBJ_VAL(string_new(vm,buffer.data + start,(int)lenght)));
        start = end + 1;
    }
    Buffer_Free(&buffer);
    return OBJ_VAL(lines);
}

static Value os_write(VM *vm, const char *name, Value *args, int argc, const char *mode){
    native_check_args(vm,name,argc,2);
    const char *path = os_text(vm,name,args[0]);
    native_check_type(vm,name,args[1],"str");

    FILE *file = fopen(path,mode);
    if(file == NULL)
        return BOOL_VAL(false);

    ObjString *text = AS_STRING(args[1]);
    bool done = fwrite(text->chars,1,(size_t)text->length,file) == (size_t)text->length;
    if(fclose(file) != 0)
        done = false;
    return BOOL_VAL(done);
}

static Value os_ecrire(VM *vm, int argc, Value *args){
    return os_write(vm,"ecrire",args,argc,"wb");
}

static Value os_ajouter(VM *vm, int argc, Value *args){
    return os_write(vm,"ajouter",args,argc,"ab");
}

static Value os_existe(VM *vm, int argc, Value *args){
    native_check_args(vm,"existe",argc,1);
    struct stat info;
    return BOOL_VAL(stat(os_text(vm,"existe",args[0]),&info) == 0);
}

static Value os_est_dossier(VM *vm, int argc, Value *args){
    native_check_args(vm,"est_dossier",argc,1);
    struct stat info;
    return BOOL_VAL(stat(os_text(vm,"est_dossier",args[0]),&info) == 0 && S_ISDIR(info.st_mode));
}

// taille("a.txt") : le nombre d'octets, ou none
static Value os_taille(VM *vm, int argc, Value *args){
    native_check_args(vm,"taille",argc,1);
    struct stat info;
    if(stat(os_text(vm,"taille",args[0]),&info) != 0)
        return NONE_VAL;
    return NUMBER_VAL((double)info.st_size);
}

static int os_compare_names(const void *left, const void *right){
    return strcmp(AS_CSTRING(*(const Value*)left),AS_CSTRING(*(const Value*)right));
}

// dossier(".") : les noms triés, sans `.` ni `..`, ou none
static Value os_dossier(VM *vm, int argc, Value *args){
    native_check_args(vm,"dossier",argc,1);

    DIR *directory = opendir(os_text(vm,"dossier",args[0]));
    if(directory == NULL)
        return NONE_VAL;

    ObjList *names = list_new(vm);
    struct dirent *entry;
    while((entry = readdir(directory)) != NULL){
        if(strcmp(entry->d_name,".") == 0 || strcmp(entry->d_name,"..") == 0)
            continue;
        list_append(vm,names,OBJ_VAL(string_new(vm,entry->d_name,(int)strlen(entry->d_name))));
    }
    closedir(directory);

    // readdir donne les noms dans n'importe quel ordre : on veut le même résultat partout.
    qsort(names->items,(size_t)names->count,sizeof(Value),os_compare_names);
    return OBJ_VAL(names);
}

// creer_dossier("a/b/c") crée aussi les dossiers parents, et accepte un dossier qui existe déjà.
static Value os_creer_dossier(VM *vm, int argc, Value *args){
    native_check_args(vm,"creer_dossier",argc,1);

    const char *text = os_text(vm,"creer_dossier",args[0]);
    if(text[0] == '\0')
        return BOOL_VAL(false);

    char *path = strdup(text);

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
    return BOOL_VAL(done);
}

// supprimer enlève un fichier ou un dossier vide : jamais un dossier avec son contenu.
static Value os_supprimer(VM *vm, int argc, Value *args){
    native_check_args(vm,"supprimer",argc,1);
    return BOOL_VAL(remove(os_text(vm,"supprimer",args[0])) == 0);
}

static Value os_renommer(VM *vm, int argc, Value *args){
    native_check_args(vm,"renommer",argc,2);
    return BOOL_VAL(rename(os_text(vm,"renommer",args[0]),os_text(vm,"renommer",args[1])) == 0);
}

static Value os_copier(VM *vm, int argc, Value *args){
    native_check_args(vm,"copier",argc,2);
    const char *target = os_text(vm,"copier",args[1]);

    FILE *input = fopen(os_text(vm,"copier",args[0]),"rb");
    if(input == NULL)
        return BOOL_VAL(false);

    FILE *output = fopen(target,"wb");
    if(output == NULL){
        fclose(input);
        return BOOL_VAL(false);
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
    return BOOL_VAL(done);
}

// chemin("a", "b", "c.txt") donne "a/b/c.txt", sans doubler les `/`
static Value os_chemin(VM *vm, int argc, Value *args){
    buffer_t buffer;
    newBuffer(&buffer);

    for(int i = 0; i < argc; i++){
        native_check_type(vm,"chemin",args[i],"str");
        ObjString *part = AS_STRING(args[i]);
        if(part->length == 0)
            continue;
        if(buffer.size > 0 && buffer.data[buffer.size - 1] != '/' && part->chars[0] != '/')
            Buffer_Append_Char(&buffer,'/');
        if(buffer.size > 0 && buffer.data[buffer.size - 1] == '/' && part->chars[0] == '/')
            Buffer_Append(&buffer,part->chars + 1,(size_t)part->length - 1);
        else
            Buffer_Append(&buffer,part->chars,(size_t)part->length);
    }

    ObjString *result = string_new(vm,buffer.data,(int)buffer.size);
    Buffer_Free(&buffer);
    return OBJ_VAL(result);
}

static Value os_dossier_courant(VM *vm, int argc, Value *args){
    (void)args;
    native_check_args(vm,"dossier_courant",argc,0);

    char *path = getcwd(NULL,0);
    if(path == NULL)
        return NONE_VAL;
    Value result = OBJ_VAL(string_new(vm,path,(int)strlen(path)));
    free(path);
    return result;
}

static Value os_changer_dossier(VM *vm, int argc, Value *args){
    native_check_args(vm,"changer_dossier",argc,1);
    return BOOL_VAL(chdir(os_text(vm,"changer_dossier",args[0])) == 0);
}

// env("HOME") : la valeur de la variable d'environnement, ou none
static Value os_env(VM *vm, int argc, Value *args){
    native_check_args(vm,"env",argc,1);
    const char *value = getenv(os_text(vm,"env",args[0]));
    return value != NULL ? OBJ_VAL(string_new(vm,value,(int)strlen(value))) : NONE_VAL;
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
static Value os_commande(VM *vm, int argc, Value *args){
    native_check_args(vm,"commande",argc,1);
    const char *command = os_text(vm,"commande",args[0]);

    // Ce qui attend d'être affiché doit sortir avant le texte de la commande.
    fflush(stdout);
    FILE *pipe = popen(command,"r");
    if(pipe == NULL)
        return NONE_VAL;

    buffer_t buffer;
    bool done = os_read_stream(pipe,&buffer);
    int code = os_exit_code(pclose(pipe));
    if(!done)
        return NONE_VAL;

    ObjMap *result = map_new(vm);
    map_set(vm,result,OBJ_VAL(string_from(vm,"code")),NUMBER_VAL(code));
    map_set(vm,result,OBJ_VAL(string_from(vm,"sortie")),OBJ_VAL(string_new(vm,buffer.data,(int)buffer.size)));
    Buffer_Free(&buffer);
    return OBJ_VAL(result);
}

// lancer("make") : la commande affiche directement son texte, et on reçoit son code de sortie.
static Value os_lancer(VM *vm, int argc, Value *args){
    native_check_args(vm,"lancer",argc,1);
    const char *command = os_text(vm,"lancer",args[0]);
    fflush(stdout);
    return NUMBER_VAL(os_exit_code(system(command)));
}

// Même forme que la table de console.c, qui la déclare aussi.
typedef struct NATIVE_FUNCTION{
    const char *name;
    NativeFn function;
}native_function;

static const native_function Os_List[] = {
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

// Un module compilé dans l'interpréteur : chaque fonction est une globale, et tout est exporté.
static ObjModule *natives_make_module(VM *vm, const char *path, const native_function *list){
    ObjModule *module = module_new(vm,string_from(vm,path));
    for(const native_function *item = list; item->name != NULL; item++){
        int index = module_global_index(vm,module,string_from(vm,item->name));
        module->globals.values[index] = OBJ_VAL(native_new(vm,item->name,item->function));
    }
    module->exports = NULL;
    module->loaded = true;
    return module;
}

// La table de console.c.
extern const native_function Console_List[];

ObjModule *natives_standard_module(VM *vm, const char *name){
    const native_function *list = NULL;
    const char *path = NULL;

    if(strcmp(name,"os") == 0 || strcmp(name,"os.cf") == 0){
        list = Os_List;
        path = "os.cf";
    }else if(strcmp(name,"console") == 0 || strcmp(name,"console.cf") == 0){
        list = Console_List;
        path = "console.cf";
    }else{
        return NULL;
    }

    // Un module n'est rempli qu'une fois : les imports suivants retrouvent le même.
    ObjString *key = string_from(vm,path);
    Value module;
    if(table_get(&vm->native_modules,key,&module))
        return AS_MODULE(module);

    ObjModule *created = natives_make_module(vm,path,list);
    table_set(&vm->native_modules,key,OBJ_VAL(created));
    return created;
}
