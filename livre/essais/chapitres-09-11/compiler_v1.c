/*
    Un premier compilateur : des expressions, des appels de builtins, et rien d'autre.
    Il remplace src/vm/compiler.c le temps d'un essai.
*/
#include "compiler.h"
#include "error.h"

typedef struct{
    VM *vm;
    ObjFunction *function;      // la fonction qu'on remplit
}Compiler;

static void compile_expr(Compiler *c, ast_t *node);

static Chunk *current_chunk(Compiler *c){
    return &c->function->chunk;
}

static void emit_byte(Compiler *c, uint8_t byte, ast_t *node){
    chunk_write(current_chunk(c),byte,node);
}

static void emit_op(Compiler *c, OpCode op, ast_t *node){
    emit_byte(c,(uint8_t)op,node);
}

static void emit_op_u8(Compiler *c, OpCode op, int value, ast_t *node){
    emit_op(c,op,node);
    emit_byte(c,(uint8_t)value,node);
}

static void emit_op_u16(Compiler *c, OpCode op, int value, ast_t *node){
    emit_op(c,op,node);
    emit_byte(c,(uint8_t)((value >> 8) & 0xFF),node);
    emit_byte(c,(uint8_t)(value & 0xFF),node);
}

static int make_constant(Compiler *c, Value value, ast_t *node){
    int index = chunk_add_constant(current_chunk(c),value);
    if(index > 0xFFFF)
        visitor_error(node,"trop de constantes dans une fonction (65536 au plus)");
    return index;
}

static OpCode binary_op(TokenEnum op){
    switch(op){
        case TOKEN_PLUS: return OP_ADD;
        case TOKEN_MINUS: return OP_SUBTRACT;
        case TOKEN_MULT: return OP_MULTIPLY;
        case TOKEN_DIV: return OP_DIVIDE;
        case TOKEN_MODULO: return OP_MODULO;
        case TOKEN_EQUAL_EQUAL: return OP_EQUAL;
        case TOKEN_DIF: return OP_NOT_EQUAL;
        case TOKEN_INF: return OP_LESS;
        case TOKEN_INF_EQUAL: return OP_LESS_EQUAL;
        case TOKEN_SUP: return OP_GREATER;
        default: return OP_GREATER_EQUAL;
    }
}

static void compile_expr(Compiler *c, ast_t *node){
    switch(node->type){
        case AST_NONE: emit_op(c,OP_NONE,node); break;
        case AST_BOOL: emit_op(c,node->bool_value ? OP_TRUE : OP_FALSE,node); break;
        case AST_NUMBER:
            emit_op_u16(c,OP_CONSTANT,make_constant(c,NUMBER_VAL(node->number_value),node),node);
            break;
        case AST_STRING:{
            Value text = OBJ_VAL(string_copy(c->vm,node->string_value,node->string_lenght));
            emit_op_u16(c,OP_CONSTANT,make_constant(c,text,node),node);
            break;
        }
        // Pour l'instant, un nom est forcément un builtin : println, print...
        case AST_VARIABLE:{
            Value name = OBJ_VAL(string_from(c->vm,node->variable_name));
            emit_op_u16(c,OP_GET_BUILTIN,make_constant(c,name,node),node);
            break;
        }
        case AST_FUNCTION_CALL:{
            list_t *args = node->function_call_args;
            compile_expr(c,node->left);
            for(int i = 0; i < args->Size; i++)
                compile_expr(c,List_Get(args,i));
            emit_op_u8(c,OP_CALL,args->Size,node);
            break;
        }
        case AST_BINOP:
            compile_expr(c,node->left);
            compile_expr(c,node->right);
            emit_op(c,binary_op(node->op),node);
            break;
        case AST_UNOP:
            compile_expr(c,node->right);
            emit_op(c,node->op == TOKEN_NOT ? OP_NOT : OP_NEGATE,node);
            break;
        default:
            visitor_error(node,"pas encore compilé");
    }
}

// Une instruction : une expression dont on jette la valeur.
static void compile_statement(Compiler *c, ast_t *node){
    compile_expr(c,node);
    emit_op(c,OP_POP,node);
}

ObjFunction *compile_module(VM *vm, ast_t *program, ObjModule *module){
    Compiler c = {.vm = vm,.function = function_new(vm,module)};
    c.function->definition = program;
    for(int i = 0; i < program->list_compound->Size; i++)
        compile_statement(&c,List_Get(program->list_compound,i));
    emit_op(&c,OP_NONE,program);
    emit_op(&c,OP_RETURN,program);
    return c.function;
}
