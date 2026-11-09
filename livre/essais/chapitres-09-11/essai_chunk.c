// Un chunk écrit à la main, puis désassemblé : -(1.2 + 3.4) / 5.6
#include <stdio.h>
#include "chunk.h"

int main(void){
    Chunk chunk;
    chunk_init(&chunk);

    int k = chunk_add_constant(&chunk,NUMBER_VAL(1.2));
    chunk_write(&chunk,OP_CONSTANT,NULL);
    chunk_write(&chunk,(uint8_t)(k >> 8),NULL);
    chunk_write(&chunk,(uint8_t)k,NULL);

    k = chunk_add_constant(&chunk,NUMBER_VAL(3.4));
    chunk_write(&chunk,OP_CONSTANT,NULL);
    chunk_write(&chunk,(uint8_t)(k >> 8),NULL);
    chunk_write(&chunk,(uint8_t)k,NULL);

    chunk_write(&chunk,OP_ADD,NULL);

    k = chunk_add_constant(&chunk,NUMBER_VAL(5.6));
    chunk_write(&chunk,OP_CONSTANT,NULL);
    chunk_write(&chunk,(uint8_t)(k >> 8),NULL);
    chunk_write(&chunk,(uint8_t)k,NULL);

    chunk_write(&chunk,OP_DIVIDE,NULL);
    chunk_write(&chunk,OP_NEGATE,NULL);
    chunk_write(&chunk,OP_RETURN,NULL);

    chunk_disassemble(&chunk,"essai");
    chunk_free(&chunk);
    return 0;
}
