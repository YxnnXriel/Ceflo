#include <string.h>
#include <math.h>

#include "include/value_expr.h"
#include "include/error.h"

#define MAX_REPEAT_SIZE (1024.0 * 1024 * 1024)

static bool is_numeric(value_t *value){
    return value->type == NUMBER || value->type == BOOL || value->type == CHAR;
}

static _Noreturn void value_expr_error(ast_t *node, value_t *left, TokenEnum op, value_t *right){
    visitor_error(
        node,
        "l'opérateur `%s` ne s'applique pas à %s et %s",
        token_to_string(op),
        Value_type_name(left),
        Value_type_name(right)
    );
}

// Dès qu'une chaîne est présente, `+` assemble le texte des deux valeurs.
static value_t *value_concat(visitor_t *visitor, value_t *left, value_t *right){
    buffer_t buffer;
    newBuffer(&buffer);
    Value_to_buffer(visitor,&buffer,left);
    Value_to_buffer(visitor,&buffer,right);
    value_t *result = makeStringLenght(buffer.data,(int)buffer.size);
    Buffer_Free(&buffer);
    return result;
}

static value_t *value_list_concat(value_t *left, value_t *right){
    value_t *result = makeList();
    for(int i = 0; i < AS_LIST(left)->Size; i++)
        Value_list_append(result,AS_LIST(left)->Item[i]);
    for(int i = 0; i < AS_LIST(right)->Size; i++)
        Value_list_append(result,AS_LIST(right)->Item[i]);
    return result;
}

// "ab" * 3 donne "ababab"
static value_t *value_repeat(ast_t *node, value_t *text, double count){
    string_t *str = AS_STRING(text);

    if(count > 0 && str->lenght * count > MAX_REPEAT_SIZE)
        visitor_error(node,"chaîne trop longue");

    int times = count > 0 ? (int)count : 0;

    buffer_t buffer;
    newBuffer(&buffer);
    for(int i = 0; i < times; i++)
        Buffer_Append(&buffer,str->str,(size_t)str->lenght);
    value_t *result = makeStringLenght(buffer.data,(int)buffer.size);
    Buffer_Free(&buffer);
    return result;
}

static value_t *value_arithmetic(ast_t *node, double left, TokenEnum op, double right){
    switch(op){
        case TOKEN_PLUS: return makeNumber(left + right);
        case TOKEN_MINUS: return makeNumber(left - right);
        case TOKEN_MULT: return makeNumber(left * right);
        case TOKEN_DIV:
            if(right == 0)
                visitor_error(node,"division par zéro");
            return makeNumber(left / right);
        case TOKEN_MODULO:
            if(right == 0)
                visitor_error(node,"modulo par zéro");
            return makeNumber(fmod(left,right));
        default:
            return NULL;
    }
}

static bool value_order_holds(double order, TokenEnum op){
    switch(op){
        case TOKEN_INF: return order < 0;
        case TOKEN_INF_EQUAL: return order <= 0;
        case TOKEN_SUP: return order > 0;
        default: return order >= 0;
    }
}

static value_t *value_compare(ast_t *node, value_t *left, TokenEnum op, value_t *right){
    if(is_numeric(left) && is_numeric(right)){
        double a = Value_as_double(left);
        double b = Value_as_double(right);
        return makeBool(value_order_holds(a < b ? -1 : (a > b ? 1 : 0),op));
    }

    bool is_text = false;
    int order = Value_compare_text(left,right,&is_text);
    if(!is_text)
        value_expr_error(node,left,op,right);

    return makeBool(value_order_holds(order,op));
}

value_t *value_expr(visitor_t *visitor, ast_t *node, value_t *left, TokenEnum op, value_t *right){
    switch(op){
        case TOKEN_EQUAL_EQUAL:
            return makeBool(Value_equals(left,right));
        case TOKEN_DIF:
            return makeBool(!Value_equals(left,right));
        case TOKEN_INF:
        case TOKEN_INF_EQUAL:
        case TOKEN_SUP:
        case TOKEN_SUP_EQUAL:
            return value_compare(node,left,op,right);
        case TOKEN_PLUS:
            if(left->type == STRING || right->type == STRING)
                return value_concat(visitor,left,right);
            if(left->type == LIST && right->type == LIST)
                return value_list_concat(left,right);
            break;
        case TOKEN_MULT:
            if(left->type == STRING && right->type == NUMBER)
                return value_repeat(node,left,AS_NUMBER(right));
            break;
        default:
            break;
    }

    if(!is_numeric(left) || !is_numeric(right))
        value_expr_error(node,left,op,right);

    return value_arithmetic(node,Value_as_double(left),op,Value_as_double(right));
}
