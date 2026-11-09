// Une première VM : une pile, un ip, et un switch.
#include <stdio.h>
#include "chunk.h"

#define PILE_MAX 256

typedef struct{
    Chunk *chunk;
    uint8_t *ip;            // le prochain octet à lire
    Value pile[PILE_MAX];
    Value *sommet;          // la première case libre
}MiniVM;

static void push(MiniVM *vm, Value v){ *vm->sommet++ = v; }
static Value pop(MiniVM *vm){ return *--vm->sommet; }

static void affiche_pile(MiniVM *vm){
    printf("          ");
    if(vm->sommet == vm->pile)
        printf("(vide)");
    for(Value *v = vm->pile; v < vm->sommet; v++)
        printf("[ %g ]",AS_NUMBER(*v));
    printf("\n");
}

static Value run(MiniVM *vm){
    #define READ_BYTE() (*vm->ip++)
    #define READ_SHORT() (vm->ip += 2,(uint16_t)((vm->ip[-2] << 8) | vm->ip[-1]))
    #define BINARY(op) do{ double b = AS_NUMBER(pop(vm)); double a = AS_NUMBER(pop(vm)); \
                           push(vm,NUMBER_VAL(a op b)); }while(0)
    for(;;){
        affiche_pile(vm);
        chunk_disassemble_instruction(vm->chunk,(int)(vm->ip - vm->chunk->code));
        switch(READ_BYTE()){
            case OP_CONSTANT: push(vm,vm->chunk->constants.values[READ_SHORT()]); break;
            case OP_ADD:      BINARY(+); break;
            case OP_SUBTRACT: BINARY(-); break;
            case OP_MULTIPLY: BINARY(*); break;
            case OP_DIVIDE:   BINARY(/); break;
            case OP_NEGATE:   push(vm,NUMBER_VAL(-AS_NUMBER(pop(vm)))); break;
            case OP_RETURN:   return pop(vm);
            default:
                printf("instruction inconnue\n");
                return NONE_VAL;
        }
    }
    #undef READ_BYTE
    #undef READ_SHORT
    #undef BINARY
}

static void emet_constante(Chunk *chunk, double x){
    int k = chunk_add_constant(chunk,NUMBER_VAL(x));
    chunk_write(chunk,OP_CONSTANT,NULL);
    chunk_write(chunk,(uint8_t)(k >> 8),NULL);
    chunk_write(chunk,(uint8_t)k,NULL);
}

int main(void){
    Chunk chunk;
    chunk_init(&chunk);
    emet_constante(&chunk,1.2);
    emet_constante(&chunk,3.4);
    chunk_write(&chunk,OP_ADD,NULL);
    emet_constante(&chunk,5.6);
    chunk_write(&chunk,OP_DIVIDE,NULL);
    chunk_write(&chunk,OP_NEGATE,NULL);
    chunk_write(&chunk,OP_RETURN,NULL);

    MiniVM vm = {.chunk = &chunk,.ip = chunk.code};
    vm.sommet = vm.pile;
    Value resultat = run(&vm);
    printf("résultat : %g\n",AS_NUMBER(resultat));
    chunk_free(&chunk);
    return 0;
}
