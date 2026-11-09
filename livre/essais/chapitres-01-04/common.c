#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
    #include <sys/resource.h>
#endif

#include "include/common.h"

#define STACK_WANTED (64UL * 1024 * 1024)
#define STACK_DEFAULT (1024UL * 1024)
#define STACK_MARGIN (256UL * 1024)

static char *stack_base;
static size_t stack_limit = STACK_DEFAULT;

void *ceflo_alloc(size_t size){
    void *pointer = calloc(1,size > 0 ? size : 1);
    if(pointer == NULL){
        fprintf(stderr,"erreur: mémoire insuffisante\n");
        exit(1);
    }
    return pointer;
}

#define POOL_BLOCK_ITEMS 512

struct POOL_BLOCK{
    struct POOL_BLOCK *next;
    max_align_t data[];
};

void *pool_alloc(pool_t *pool, size_t size){
    #ifdef CEFLO_NO_POOL
        (void)pool;
        return ceflo_alloc(size);
    #else
        if(pool->free_items != NULL){
            void *item = pool->free_items;
            pool->free_items = *(void**)item;
            memset(item,0,size);
            return item;
        }

        if(pool->left == 0){
            struct POOL_BLOCK *block = ceflo_alloc(sizeof(*block) + POOL_BLOCK_ITEMS * size);
            block->next = pool->blocks;
            pool->blocks = block;
            pool->left = POOL_BLOCK_ITEMS;
        }

        pool->left -= 1;
        return (char*)pool->blocks->data + pool->left * size;
    #endif
}

void pool_release(pool_t *pool, void *item){
    #ifdef CEFLO_NO_POOL
        (void)pool;
        free(item);
    #else
        *(void**)item = pool->free_items;
        pool->free_items = item;
    #endif
}

void pool_free_all(pool_t *pool){
    while(pool->blocks != NULL){
        struct POOL_BLOCK *next = pool->blocks->next;
        free(pool->blocks);
        pool->blocks = next;
    }
    pool->free_items = NULL;
    pool->left = 0;
}

void *ceflo_realloc(void *pointer, size_t size){
    pointer = realloc(pointer,size > 0 ? size : 1);
    if(pointer == NULL){
        fprintf(stderr,"erreur: mémoire insuffisante\n");
        exit(1);
    }
    return pointer;
}

void allowChar(char **chaine, const char *value){
    *chaine = ceflo_alloc(strlen(value) + 1);
    strcpy(*chaine,value);
}

/**
 * @brief retient le bas de la pile et l'agrandit quand le système le permet
 */
void ceflo_stack_init(void){
    stack_base = __builtin_frame_address(0);

    #ifndef _WIN32
        struct rlimit limit;
        if(getrlimit(RLIMIT_STACK,&limit) == 0){
            if(limit.rlim_cur != RLIM_INFINITY && limit.rlim_cur < STACK_WANTED &&
                (limit.rlim_max == RLIM_INFINITY || limit.rlim_max >= STACK_WANTED)){
                limit.rlim_cur = STACK_WANTED;
                setrlimit(RLIMIT_STACK,&limit);
            }
            if(getrlimit(RLIMIT_STACK,&limit) == 0){
                stack_limit = limit.rlim_cur == RLIM_INFINITY || limit.rlim_cur > STACK_WANTED ?
                    STACK_WANTED : limit.rlim_cur;
            }
        }
    #endif
}

bool ceflo_stack_exceeded(void){
    char *here = __builtin_frame_address(0);
    size_t used = here < stack_base ? (size_t)(stack_base - here) : (size_t)(here - stack_base);
    return used + STACK_MARGIN > stack_limit;
}
