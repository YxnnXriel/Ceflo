#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "memory.h"
#include "common.h"
#include "str.h"

/*
    Les opérateurs quand les deux côtés ne sont pas des nombres : les règles et
    les messages de src/value_expr.c. Le cas nombre/nombre est fait par la VM.
*/

#define MAX_REPEAT_SIZE (1024.0 * 1024 * 1024)

static const char *binary_symbol(OpCode op){
    switch(op){
        case OP_ADD: return "+";
        case OP_SUBTRACT: return "-";
        case OP_MULTIPLY: return "*";
        case OP_DIVIDE: return "/";
        case OP_MODULO: return "%";
        case OP_EQUAL: return "==";
        case OP_NOT_EQUAL: return "!=";
        case OP_LESS: return "<";
        case OP_LESS_EQUAL: return "<=";
        case OP_GREATER: return ">";
        case OP_GREATER_EQUAL: return ">=";
        default: return "?";
    }
}

static _Noreturn void binary_error(VM *vm, OpCode op, Value a, Value b){
    vm_error(vm,"l'opérateur `%s` ne s'applique pas à %s et %s",binary_symbol(op),value_type_name(a),value_type_name(b));
}

static bool binary_is_numeric(Value value){
    return IS_NUMBER(value) || IS_BOOL(value) || IS_CHAR(value);
}

static double binary_as_double(Value value){
    switch(value.type){
        case VAL_NUMBER: return AS_NUMBER(value);
        case VAL_BOOL: return AS_BOOL(value) ? 1 : 0;
        case VAL_CHAR: return (unsigned char)AS_CHAR(value);
        default: return 0;
    }
}

// Un char se lit comme un texte d'un seul octet.
static bool binary_text_view(Value *value, const char **text, int *length){
    if(IS_STRING(*value)){
        *text = AS_CSTRING(*value);
        *length = AS_STRING(*value)->length;
        return true;
    }
    if(IS_CHAR(*value)){
        *text = &AS_CHAR(*value);
        *length = 1;
        return true;
    }
    return false;
}


//                          ===================================== $TEXTE ==============================================


// Le texte d'un côté de `+` : une str se lit telle quelle, le reste s'écrit dans un tampon.
typedef struct{
    const char *chars;
    size_t length;
    buffer_t buffer;    // data reste NULL si le texte n'a pas eu besoin de tampon
}TextPart;

static void binary_text(VM *vm, TextPart *part, Value *value){
    part->buffer.data = NULL;
    if(IS_STRING(*value)){
        part->chars = AS_CSTRING(*value);
        part->length = (size_t)AS_STRING(*value)->length;
    }else if(IS_CHAR(*value)){
        part->chars = &AS_CHAR(*value);
        part->length = 1;
    }else{
        newBuffer(&part->buffer);
        vm_value_to_buffer(vm,&part->buffer,*value);
        part->chars = part->buffer.data;
        part->length = part->buffer.size;
    }
}

// Un seul memcpy par côté, directement dans la chaîne finale.
static ObjString *binary_join(VM *vm, const char *a, size_t length_a, const char *b, size_t length_b){
    size_t length = length_a + length_b;
    if(length > (size_t)INT32_MAX)
        vm_error(vm,"chaîne trop longue");

    // Un texte court est interné : string_new le cherche dans la table.
    char small[64];
    if(length <= sizeof(small)){
        memcpy(small,a,length_a);
        memcpy(small + length_a,b,length_b);
        return string_new(vm,small,(int)length);
    }

    ObjString *result = string_reserve(vm,(int)length);
    memcpy(result->chars,a,length_a);
    memcpy(result->chars + length_a,b,length_b);
    return result;
}

// Dès qu'une chaîne est présente, `+` assemble le texte des deux valeurs.
static Value binary_concat(VM *vm, Value a, Value b){
    // Un __str__ peut tourner pendant l'affichage de a : b doit rester en vie.
    vm_push(vm,a);
    vm_push(vm,b);
    TextPart left, right;
    binary_text(vm,&left,&a);
    binary_text(vm,&right,&b);
    ObjString *result = binary_join(vm,left.chars,left.length,right.chars,right.length);
    free(left.buffer.data);
    free(right.buffer.data);
    vm->stack_top -= 2;
    return OBJ_VAL(result);
}

static Value binary_list_concat(VM *vm, ObjList *a, ObjList *b){
    ObjList *result = list_new(vm);
    int count = a->count + b->count;
    if(count > 0){
        result->items = VM_ALLOCATE(vm,Value,count);
        result->capacity = count;
        memcpy(result->items,a->items,sizeof(Value) * (size_t)a->count);
        memcpy(result->items + a->count,b->items,sizeof(Value) * (size_t)b->count);
        result->count = count;
    }
    return OBJ_VAL(result);
}

// "ab" * 3 donne "ababab"
static Value binary_repeat(VM *vm, ObjString *text, double count){
    if(count > 0 && text->length * count > MAX_REPEAT_SIZE)
        vm_error(vm,"chaîne trop longue");

    int times = count > 0 ? (int)count : 0;
    int length = text->length * times;
    char small[64];
    char *chars = small;
    ObjString *result = NULL;
    if(length > (int)sizeof(small)){
        result = string_reserve(vm,length);
        chars = result->chars;
    }
    for(int i = 0; i < times; i++)
        memcpy(chars + i * text->length,text->chars,(size_t)text->length);
    return OBJ_VAL(result != NULL ? result : string_new(vm,small,length));
}


//                          ===================================== $COMPARAISONS ==============================================


static bool binary_order_holds(int order, OpCode op){
    switch(op){
        case OP_LESS: return order < 0;
        case OP_LESS_EQUAL: return order <= 0;
        case OP_GREATER: return order > 0;
        default: return order >= 0;
    }
}

static Value binary_compare(VM *vm, OpCode op, Value a, Value b){
    if(binary_is_numeric(a) && binary_is_numeric(b)){
        double x = binary_as_double(a);
        double y = binary_as_double(b);
        return BOOL_VAL(binary_order_holds(x < y ? -1 : (x > y ? 1 : 0),op));
    }

    const char *text_a, *text_b;
    int length_a, length_b;
    if(!(IS_STRING(a) || IS_STRING(b)) ||
        !binary_text_view(&a,&text_a,&length_a) || !binary_text_view(&b,&text_b,&length_b))
        binary_error(vm,op,a,b);

    int order = memcmp(text_a,text_b,(size_t)(length_a < length_b ? length_a : length_b));
    if(order == 0)
        order = length_a - length_b;
    return BOOL_VAL(binary_order_holds(order,op));
}


//                          ===================================== $OPERATEURS ==============================================


Value vm_binary(VM *vm, OpCode op, Value a, Value b){
    switch(op){
        case OP_EQUAL: return BOOL_VAL(value_equals(a,b));
        case OP_NOT_EQUAL: return BOOL_VAL(!value_equals(a,b));
        case OP_LESS:
        case OP_LESS_EQUAL:
        case OP_GREATER:
        case OP_GREATER_EQUAL:
            return binary_compare(vm,op,a,b);
        case OP_ADD:
            if(IS_STRING(a) || IS_STRING(b))
                return binary_concat(vm,a,b);
            if(IS_LIST(a) && IS_LIST(b))
                return binary_list_concat(vm,AS_LIST(a),AS_LIST(b));
            break;
        case OP_MULTIPLY:
            if(IS_STRING(a) && IS_NUMBER(b))
                return binary_repeat(vm,AS_STRING(a),AS_NUMBER(b));
            break;
        default:
            break;
    }

    if(!binary_is_numeric(a) || !binary_is_numeric(b))
        binary_error(vm,op,a,b);

    double x = binary_as_double(a);
    double y = binary_as_double(b);
    switch(op){
        case OP_ADD: return NUMBER_VAL(x + y);
        case OP_SUBTRACT: return NUMBER_VAL(x - y);
        case OP_MULTIPLY: return NUMBER_VAL(x * y);
        case OP_DIVIDE:
            if(y == 0)
                vm_error(vm,"division par zéro");
            return NUMBER_VAL(x / y);
        case OP_MODULO:
            if(y == 0)
                vm_error(vm,"modulo par zéro");
            return NUMBER_VAL(fmod(x,y));
        default:
            binary_error(vm,op,a,b);
    }
}
