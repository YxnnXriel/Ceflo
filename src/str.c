#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "include/str.h"


/**
 * @brief Constructeur de String
 * 
 * @param str 
 * @return string 
 */
 
String new_String(const char *str){

    String st = calloc(1,sizeof(*st));

    if(st == NULL){
        fprintf(stderr,"line :%d , files :%s , function : %s\n",
            __LINE__,
            __FILE__,
            __func__
        );
        exit(1);
    }

    if(str != NULL){
        st->str = calloc(strlen(str) + 1, sizeof(char*));
        strcpy(st->str,str);
        st->lenght = strlen(str); 
    }else{

        st->str = NULL;
        st->lenght = 0;
    }

    return st;    
}


/**
 * @brief Donne le caractère actuelle
 * 
 * @param str 
 * @param index 
 * @return char 
 */
 
char String_index(String str , int index){
    return str->lenght >= index ? str->str[index] : 0;
}


char * String_index_ptr(String str , int index){
    return str->lenght >= index ? &str->str[index] : NULL;
}


/**
 * @brief Retourne la chaine actuelle,Parfait pour L'affichage
 * 
 * @param str 
 * @return char* 
 */
 
char *String_str(String str){
    return str->str != NULL ? str->str:NULL;
}


/**
 * @brief Renvoi la longueur de la chaine actuelle
 * 
 * @param str 
 * @return int 
 */
 
int String_lenght(String str){
    return str->lenght;
}

/**
 * @brief Modifie la chaine actuelle
 * 
 * @param str 
 * @param new_str 
 * @return int 
 */

 
int String_Set(String str , char *new_str){

    if(new_str == (void*)0){
        free(str->str);
        str->str = NULL;
        str->lenght = 0;
        return 0;
    }else{
        free(str->str);
        str->str = calloc((strlen(new_str)+ 1) ,sizeof(char));
        if(str->str == (void*)0){
            fprintf(stderr,"line :%d , files :%s , function : %s -> probleme Set::String\n",
                __LINE__,
                __FILE__,
                __func__
            );
            exit(1);
        }
        strcpy(str->str,new_str);
        str->lenght = strlen(new_str);
        return str->lenght == 0 ? 0:str->lenght;
    }
}

/**
 * @brief Modifie le caractere de la chaine à l'index donner
 * 
 * @param str 
 * @param index 
 * @param carac 
 * @return int 
 */
 
int String_Set_char(String str , int index , char carac){
    if (str->lenght >= index)
    {
        str->str[index] = carac;
        return index;
    }
    return -1;
}


/**
 * @brief Decoupe la chaine et renvoi sous forme de liste de chaine de caractère
 * 
 * @param str 
 * @param delim 
 * @return listString 
 */
 
listString String_Split(String str , char *delim){
    
    char *s = String_str(str);
    listString list = new_ListString();

    for (char *Tok = strtok(s,delim); Tok != NULL ; Tok = strtok(NULL,delim))
        ListString_Append(list,Tok);

    return list;
}

 
String String_Remont(listString list ,char *delim){
    
    int len = list->Size;
    
    for (int i = 0; i < list->Size; i++)
    {
        len += ListString_lenght(list,i);
    }

    char *s = calloc(len,sizeof(char));

    for (int i = 0; i < list->Size; i++)
    {
        strcat(s,ListString_Get(list,i));
        
        if(delim != NULL)
            strcat(s,delim);
    }

    String str = new_String(s);
    free(s);

    // ListString_Clear(list);
    return str; 

}




int String_cmp(string_t *string , const char *chaine){
    if(strcmp(string->str,chaine) == 0)
        return 1;
    return 0;
}


String String_cat(char *format , ...){
    /**
    *  %s string format
    *  %d int format
    *  %f float format
    */

    va_list args;
    
    va_start(args,format);
    char Buffer[BUFSIZ];

    
    vsprintf(Buffer,format,args);
    va_end(args);
    string_t *chaine = new_String(Buffer);
    return chaine;
}

char* readFile(const char *path){
	FILE *file = fopen(path,"rb");

	if(file == NULL){
		fprintf(stderr,"%s: Impossible d'ouvrir le fichier \" %s \" .\n",__func__,path);
		exit(74);
	}

	fseek(file,0,SEEK_END);
	size_t fileSize = ftell(file);
	rewind(file);

	char *buffer = calloc(fileSize+1,sizeof(char*));

	if(buffer == NULL){
		fprintf(stderr,"%s: Memoire insuffisante pour lire \" %s \" .\n",__func__,path);
		exit(74);
	}

	size_t byteRead = fread(buffer,sizeof(char) , fileSize,file);

	if(byteRead < fileSize){
		fprintf(stderr,"%s: Impossible de lire le fichier \" %s \" .\n",__func__,path);
		exit(74);
	}
	buffer[byteRead] = '\0';
	fclose(file);


    return buffer;
}


/**
 * @brief lit dans le fichiers dans une string 
 * @param args 
 * @return String 
 */

String String_Read_FIle(const char *args){
    return new_String(readFile(args));
}



String String_Reverse(String chaine){
	String ch = new_String(chaine->str);
	for (int i = ch->lenght; i > -1 ; i--)
		String_Set_char(chaine,(ch->lenght-1)- i,ch->str[i]);

    String_Set(ch,chaine->str);
	return ch;
}



/* ------------------LIST STRING---------------------- */




/**
 * @brief Constructeur de listString
 * 
 * @return listString 
 */

listString new_ListString(){
    listString list = malloc(sizeof(*list));
    list->Size = 0;
    list->ListStr = (void *)0;
    list->Limit_Size = -1;

    return list;
}

/**
 * @brief Constructeur de listString avec limiteur
 * 
 * @return listString 
 */
listString new_ListString_limit(int x){
    listString list = malloc(sizeof(*list));
    list->Size = 0;
    list->ListStr = (void *)0;
    list->Limit_Size = x;

    return list;
}



/**
 * @brief Ajoute une chaine à la liste
 * 
 * @param list 
 * @param value 
 * @return int 
 */

 
int ListString_Append(listString list , const char *value){

        list->Size++;
        list->ListStr = list->ListStr == (void*)0 ? 
        calloc(list->Size,sizeof(list->ListStr)):
        realloc(list->ListStr, list->Size * sizeof(list->ListStr));

        list->ListStr[list->Size -1] = new_String(value);

    return 1;
}


int ListString_Append_char(listString list , char c_value){

        char *value = calloc(2,sizeof(char));
        value[0] = c_value;
        value[1] = '\0';

        list->Size++;
        list->ListStr = list->ListStr == (void*)0 ? 
        calloc(list->Size,sizeof(list->ListStr)):
        realloc(list->ListStr, list->Size * sizeof(list->ListStr));

        list->ListStr[list->Size -1] = new_String(value);

        free(value);

    return 1;
}


/**
 * @brief Renvoi la longueur de la chaine à l'index passer en parametre
 * 
 * @param list 
 * @param index 
 * @return int 
 */

int ListString_lenght(listString list , int index){
    return list->ListStr != NULL && list->Size >= index ? String_lenght(list->ListStr[index]) : 0; 
}


/**
 * @brief Renvoi la chaine dans la liste à l'indice passer en parametre
 * 
 * @param list 
 * @param index 
 * @return char* 
 */

 
char * ListString_Get(listString list , int index){
    return list->ListStr != NULL && list->Size >= index ? String_str(list->ListStr[index]) : NULL;    
}

/**
 * @brief Modifie la chaine à l'index
 * 
 * @param list 
 * @param index 
 * @param new_value 
 */
 
void ListString_Set(listString list , int index , char *new_value){
    if(list->ListStr != NULL && list->Size >= index){
        String_Set(list->ListStr[index],new_value);
    }
}


/**
 * @brief Rechercher une chaine en fonction d'une autre chaine
 * 
 * @param list 
 * @param value 
 * @return int 
 */

int ListString_Search(listString list , char *value){
    for (int i = 0; i < list->Size; i++)
        if(strcmp(value,ListString_Get(list,i)) ==0)
            return i;

    return -1;    
}


/**
 * @brief Supprime une chaine à l'index passer en parametre
 * 
 * @param list 
 * @param index 
 * @return int 
 */


int ListString_Delete(listString list, int index) {
    if(index < list->Size && index >= 0) { // Vérifie que l'index est valide

        string_t *str = list->ListStr[index];

        // Déplace les éléments vers la gauche pour remplir l'espace de l'élément supprimé
        for(int i = index; i < list->Size - 1; i++)
            list->ListStr[i] = list->ListStr[i + 1]; 
        
        // Libère la mémoire de l'élément supprimé
        String_free(str);
        
        list->Size--;

        // Réalloue de la mémoire pour la nouvelle taille de la liste
        if(list->Size != 0)
            list->ListStr = realloc(list->ListStr, sizeof(struct STRUCT_STR*) * list->Size);
        
        return 1; // Retourne 1 pour indiquer que la suppression a réussi
    }

    return 0; // Retourne 0 si l'index est invalide ou si la liste est vide
}

/**
 * @brief Supprime tous les elements de la liste
 * 
 * @param list 
 * @return int 
 */

 
int ListString_Clear(listString list){
    int x = list->Size;
    for (int i = x-1; i >= 0; i--)
    {
        ListString_Delete(list,i);
    }

    list->Size = 0;
    free(list->ListStr);
    free(list);
    return 1;
    
}


/**
 * @brief copie / duplique la liste  
 * 
 * @param list 
 * @return listString 
 */

 
listString ListString_Copy(listString list){
    if(list != NULL){
        listString new = new_ListString();
        new->ListStr = list->ListStr;
        new->Size = list->Size;
        return new;
    }
    return new_ListString();
}



/**
 * @brief free String
 * 
 * @param str 
 * @return int 
 */


int String_free(String str){
    if(str == NULL)
        return 0;
        
    if(str->str != NULL)
        free(str->str);
    
    if(str != NULL)
        free(str);

    return 1;        
}


/**
 * @brief Affiche tous les elements de la liste de String
 * 
 * @param list 
 */


void ListString_Print(listString list){

    if(list != NULL)
        if(list->Size != 0)
            for (int i = 0; i < list->Size; i++)
                printf("%s\n",ListString_Get(list,i));
}


/**
 * @brief affichage personnaliser
 * 
 * @param list 
 * @param print 
 */


void ListString_Print_mod(listString list , void (*print)(listString)){
    if((*print) == (void*)0){
        ListString_Print(list);
    }else{
        (*print)(list);
    }
}


/** --------------------------------- Ext ---------------------------------------- **/



void dessine_ligne(listString list,int online , int tab[], int eachlen[],char mid_char ){
    int j =0;

    for (int i = 0; i < online; i++){

        if(mid_char == '#'){
            if(i == 0){
                printf("╔");
            }else if(i == online-1){
                printf("═╗");
                printf("\n");
            }else{
                if(i == tab[j+1]){
                    printf("╦");
                    j++;
                }else{
                    printf("═");
                }
            }

        }else if(mid_char == '-'){
            if(i == 0){
                printf("╠");
            }else if(i == online-1){
                printf("═╣");
                printf("\n");
            }else{
                if(i == tab[j+1]){
                    printf("╬");
                    j++;
                }else{
                    printf("═");
                }
            }
        }else if(mid_char == '|'){
            if(i == tab[j]){
                printf("║ %s",ListString_Get(list,j));
                int m = eachlen[j+1] - ListString_lenght(list,j);
                for (int l = 0; l < m+1; l++)
                    printf(" ");
                    
                j++;
            }
              if(i == online-1)
                    printf("║\n");
        }else if(mid_char == '^'){
            if(i == 0){
                printf("╚");
            }else if(i == online-1){
                printf("═╝");
                printf("\n");
            }else{
                if(i == tab[j+1]){
                    printf("╩");
                    j++;
                }else{
                    printf("═");
                }
            }
        }
    }
}


/**
 * @brief Dessine un Tableau
 * 
 * @param champs 
 * @param list 
 */


void Table(String champs,listString list){

    listString champ = String_Split(champs," \n");

    int *lenTab = malloc(( champ->Size+1 )*sizeof(int));
    int *eachlen = malloc( (champ->Size+1 ) *sizeof(int));
    
    for (int i = 0; i < champ->Size; i++){
        lenTab[i] = 0;
        eachlen[i]= 0;
    }
    

    int une_ligne = 0;
    for (int i = 1; i < champ->Size+1; i++){
        for (int j = 0; j < list->Size; j++){
            char *s = ListString_Get(list,j);
            listString chaine = String_Split(new_String(s)," \n");
            int x = ListString_lenght(chaine,i-1);
            if(x > lenTab[i]){
                lenTab[i] = x;
            }
            if(lenTab[i] < ListString_lenght(champ,i-1)){
                lenTab[i] = ListString_lenght(champ,i-1);   
            }
        }
        une_ligne += lenTab[i] + 3;
        eachlen[i] = lenTab[i];
        lenTab[i] = une_ligne;
    }
    
    dessine_ligne(champ,une_ligne , lenTab,eachlen,'#');
    dessine_ligne(champ,une_ligne , lenTab,eachlen,'|');
    dessine_ligne(champ,une_ligne , lenTab,eachlen,'-');

    for (int i = 0; i < list->Size; i++){
        listString cha = String_Split(new_String(ListString_Get(list,i))," \n");
        dessine_ligne(cha,une_ligne , lenTab,eachlen,'|');
        ListString_Clear(cha);
    }
    
    dessine_ligne(champ,une_ligne , lenTab,eachlen,'^');

    ListString_Clear(champ);
}