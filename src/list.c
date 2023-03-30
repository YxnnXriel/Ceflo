#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "include/list.h"

#define TAB_SIZE 100
#define TAB_OFFSET 14695981039346656037UL
#define TAB_PRIME 1099511628211UL

list_t * __init__list__(size_t x){

    list_t *l = calloc(1,sizeof(*l));

    if(l == (void*)0){
        fprintf(stderr,"line :%d , files :%s , function : %s\n",
            __LINE__,
            __FILE__,
            __func__
        );
        exit(1);
    }

    l->Sizeof = x;
    l->Item = (void*)0;
    l->Size = 0;

    return l;
}

int List_Append(list_t *list , void *value){

    list->Size +=1;

    list->Item = list->Item == (void*)0 ?
        calloc(1,list->Sizeof): realloc(list->Item,list->Size * list->Sizeof);


    if(list->Item == (void*)0){
        fprintf(stderr,"Allocation dynamique -> line :%d , files :%s , function : %s\n",
            __LINE__,
            __FILE__,
            __func__
        );
        exit(1);
    }
    
    list->Item[list->Size -1] = value;

    return 1;
}

void * List_Get(list_t *list,int index){
    return index < list->Size ? list->Item[index] : (void*)0;
}
void * List_Get_Last(list_t *list){
    return list->Size != 0 ? list->Item[list->Size -1]:(void*)0;
}
void * List_Get_Firts(list_t *list){
    return list->Size != 0 ? list->Item[0]:(void*)0;
}



int List_Del(list_t *list , int index,void (*free_method)(void *x)){


    if(index > list->Size)
        return 0;

    // printf("    DEBUT\n");
    if(free_method != (void*)0)
        free_method(list->Item[index]);
    
    // printf("    FIN\n");
    for (int i = index; i < list->Size -1; i++)
        list->Item[i] = list->Item[i+1];
    

    void *tmp = realloc(list->Item,(list->Size -1 ) * list->Sizeof);
    if(tmp == NULL && list->Size > 1){
        fprintf(stderr,"line :%d , files :%s , function : %s\n",
            __LINE__,
            __FILE__,
            __func__
        );
        exit(1);
    }

    list->Size = list->Size -1;
    list->Item = tmp;

    return 1;

}

int List_Del_all(list_t *list ,void (*free_method)(void *x)){

    if(list->Size <= 0)
        return 1; 

    for (int i = list->Size -1; i > 0; i--){
        List_Del(list,i,free_method);
    }

    free(list);
    
    return 1;

}

int List_Del_by_item(list_t *list , void *x , void (*free_function)(void *y)){
    for (int i = 0; i < list->Size; i++){
        if(List_Get(list,i) == x){
            List_Del(list,i,free_function);
            return 1;
        }
    }
    return 0;
}


void List_Free(list_t *list){

    free(list->Item);
    free(list);
    return;
}


/* ========================================== */




uint64_t  Hash(const char *chaine){

     uint64_t v = TAB_OFFSET;

    for (int i = 0; chaine[i] != '\0'; i++){
        v ^=  (uint64_t)(unsigned char)chaine[i];
        v *= TAB_PRIME;        
    }

    return v;
}

hash_t *__init__hash(const char *key ,void * value ){

    hash_t *ha = calloc(1,sizeof(*ha));
    

    if(ha == (void*)0){
        
        fprintf(stderr,"line :%d , files :%s , function : %s\n",
            __LINE__,
            __FILE__,
            __func__
        );
        exit(1);
        
    }


    ha->key = calloc((strlen(key)) , sizeof(char));
    strcpy(ha->key,key);
    ha->value = value;


    // ha->next = (void*)0;

    return ha;
}

static void freeHash(hash_t *hash){

    if(hash != NULL){
    // printf("ICI %s \n",hash == NULL ? "oui":"non");
        if(hash->key != NULL){
            free(hash->key);
        }
        if(hash != NULL){
            free(hash);
        }
    }
}
stk_t *__init__stk_hash(stk_t *stk){

    stk->Size = 0;
    for (int i = 0; i < 100; i++){
        stk->TabHash[i] = (void*)0;    
    }

    return stk;
}

int Hash_Append(stk_t * stk ,const char *key ,void * value){
    
    hash_t *ha = __init__hash(key,value);

    if(ha == (void*)0) return 0;

    int index = Hash(ha->key) % TAB_SIZE;
    ha->next = stk->TabHash[index];
    stk->TabHash[index] = ha;


    stk->Size +=1;

    return 1;
}

void *Hash_Get(stk_t *stk , const char *key){

    int index = Hash(key) % TAB_SIZE;
    hash_t *l = stk->TabHash[index];

    while (l != (void*)0 && strcmp(l->key,key) != 0){
        l = l->next;
    }


    return l != (void*)0 ? l->value:(void*)0;
}

int Hash_Modif(stk_t *stk , const char *key,void *value){

    int index = Hash(key) % TAB_SIZE;
    hash_t *l = stk->TabHash[index];

    while (l != (void*)0 && strcmp(l->key,key) != 0){
        l = l->next;
    }

    if(l != (void*)0){
        l->value = value;
        return 1;
    }

    return 0;

}

void Hash_Print(stk_t *stk){

    for (int i = 0; i < TAB_SIZE; i++){
        if(stk->TabHash[i] != (void*)0){
            hash_t *l = stk->TabHash[i];
            if(l != NULL){
                printf("[%d]\t",i);
                while (l != (void*)0){
                    printf("%s(%p) -> ",l->key,(l->value));
                    l = l->next;
                }
                printf("\n");
            }
        }
    }   
}


void Hash_Del(stk_t *stk){

        for (int i = 0; i < TAB_SIZE; i++){
        if(stk->TabHash[i] != (void*)0){
            hash_t *l = stk->TabHash[i];
            if(l != NULL){
                // printf("[%d]\t",i);
                while (l != (void*)0){
                    hash_t * save = l;
                    l = l->next;
                    freeHash(save);
                }
                // printf("\n");
            }
        }
    }   
}
