#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "common.h"

void chunk_init(Chunk *chunk){
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->nodes = NULL;
    value_array_init(&chunk->constants);
    chunk->checks = NULL;
    chunk->check_count = 0;
    chunk->check_capacity = 0;
}

void chunk_free(Chunk *chunk){
    free(chunk->code);
    free(chunk->nodes);
    value_array_free(&chunk->constants);
    free(chunk->checks);
    chunk_init(chunk);
}

void chunk_write(Chunk *chunk, uint8_t byte, ast_t *node){
    if(chunk->count == chunk->capacity){
        chunk->capacity = chunk->capacity < 8 ? 8 : chunk->capacity * 2;
        chunk->code = ceflo_realloc(chunk->code,(size_t)chunk->capacity);
        chunk->nodes = ceflo_realloc(chunk->nodes,sizeof(ast_t*) * (size_t)chunk->capacity);
    }
    chunk->code[chunk->count] = byte;
    chunk->nodes[chunk->count] = node;
    chunk->count += 1;
}

// Deux nombres sont la même constante s'ils ont les mêmes bits : 0 et -0 restent distincts.
static bool chunk_same_constant(Value a, Value b){
    if(IS_NUMBER(a) && IS_NUMBER(b))
        return memcmp(&AS_NUMBER(a),&AS_NUMBER(b),sizeof(double)) == 0;
    return IS_STRING(a) && IS_STRING(b) && AS_OBJ(a) == AS_OBJ(b);
}

int chunk_add_constant(Chunk *chunk, Value value){
    if(IS_NUMBER(value) || IS_STRING(value)){
        for(int i = 0; i < chunk->constants.count; i++){
            if(chunk_same_constant(chunk->constants.values[i],value))
                return i;
        }
    }
    value_array_write(&chunk->constants,value);
    return chunk->constants.count - 1;
}

int chunk_add_check(Chunk *chunk, TypeCheck check){
    if(chunk->check_count == chunk->check_capacity){
        chunk->check_capacity = chunk->check_capacity < 8 ? 8 : chunk->check_capacity * 2;
        chunk->checks = ceflo_realloc(chunk->checks,sizeof(TypeCheck) * (size_t)chunk->check_capacity);
    }
    chunk->checks[chunk->check_count] = check;
    chunk->check_count += 1;
    return chunk->check_count - 1;
}
