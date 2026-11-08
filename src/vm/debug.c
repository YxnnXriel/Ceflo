/**
 * @file debug.c
 * @brief le désassembleur : `ceflo --dis fichier.cf` affiche le bytecode.
 *
 * [NOTE]
 *      Une ligne par instruction :
 *
 *          0004     3  GET_LOCAL          1
 *          0006     |  CONSTANT           2  1
 *          0009     |  ADD
 *
 *      l'offset dans le code, la ligne du source (`|` si c'est la même que
 *      l'instruction d'avant), le nom, les opérandes, et la valeur de la
 *      constante quand l'instruction en lit une.
 */

#include <stdio.h>

#include "chunk.h"
#include "object.h"

static const char *opcode_names[OP_COUNT] = {
    [OP_CONSTANT] = "CONSTANT",
    [OP_NONE] = "NONE",
    [OP_TRUE] = "TRUE",
    [OP_FALSE] = "FALSE",
    [OP_POP] = "POP",
    [OP_DUP] = "DUP",
    [OP_DUP2] = "DUP2",
    [OP_GET_LOCAL] = "GET_LOCAL",
    [OP_SET_LOCAL] = "SET_LOCAL",
    [OP_GET_UPVALUE] = "GET_UPVALUE",
    [OP_SET_UPVALUE] = "SET_UPVALUE",
    [OP_GET_GLOBAL] = "GET_GLOBAL",
    [OP_SET_GLOBAL] = "SET_GLOBAL",
    [OP_DEFINE_GLOBAL] = "DEFINE_GLOBAL",
    [OP_GET_BUILTIN] = "GET_BUILTIN",
    [OP_CLOSE_UPVALUE] = "CLOSE_UPVALUE",
    [OP_CHECK_TYPE] = "CHECK_TYPE",
    [OP_ADD] = "ADD",
    [OP_SUBTRACT] = "SUBTRACT",
    [OP_MULTIPLY] = "MULTIPLY",
    [OP_DIVIDE] = "DIVIDE",
    [OP_MODULO] = "MODULO",
    [OP_EQUAL] = "EQUAL",
    [OP_NOT_EQUAL] = "NOT_EQUAL",
    [OP_LESS] = "LESS",
    [OP_LESS_EQUAL] = "LESS_EQUAL",
    [OP_GREATER] = "GREATER",
    [OP_GREATER_EQUAL] = "GREATER_EQUAL",
    [OP_NEGATE] = "NEGATE",
    [OP_NOT] = "NOT",
    [OP_TO_BOOL] = "TO_BOOL",
    [OP_JUMP] = "JUMP",
    [OP_JUMP_IF_FALSE] = "JUMP_IF_FALSE",
    [OP_LOOP] = "LOOP",
    [OP_CALL] = "CALL",
    [OP_INVOKE] = "INVOKE",
    [OP_CLOSURE] = "CLOSURE",
    [OP_RETURN] = "RETURN",
    [OP_LIST] = "LIST",
    [OP_MAP] = "MAP",
    [OP_GET_INDEX] = "GET_INDEX",
    [OP_SET_INDEX] = "SET_INDEX",
    [OP_GET_FIELD] = "GET_FIELD",
    [OP_SET_FIELD] = "SET_FIELD",
    [OP_FORMAT] = "FORMAT",
    [OP_STRUCT] = "STRUCT",
    [OP_METHOD] = "METHOD",
    [OP_FIELDS_INIT] = "FIELDS_INIT",
    [OP_NEW] = "NEW",
    [OP_TYPE_CALL] = "TYPE_CALL",
    [OP_ENUM] = "ENUM",
    [OP_ITER_PREP] = "ITER_PREP",
    [OP_ITER_NEXT] = "ITER_NEXT",
    [OP_MATCH_FAIL] = "MATCH_FAIL",
    [OP_IMPORT] = "IMPORT",
    [OP_IMPORT_NAME] = "IMPORT_NAME",
    [OP_IMPORT_VALUE] = "IMPORT_VALUE",
    [OP_IMPORT_ALL] = "IMPORT_ALL",
    [OP_IMPORT_NATIVE] = "IMPORT_NATIVE",
    [OP_EXPORT] = "EXPORT",
    [OP_SET_LOCAL_POP] = "SET_LOCAL_POP",
    [OP_ADD_LOCAL] = "ADD_LOCAL",
    [OP_GET_SELF_FIELD] = "GET_SELF_FIELD",
    [OP_SET_SELF_FIELD] = "SET_SELF_FIELD",
};

const char *opcode_name(OpCode op){
    if(op >= OP_COUNT || opcode_names[op] == NULL)
        return "???";
    return opcode_names[op];
}

// Une constante, en court : le désassembleur n'a pas de VM pour appeler __str__.
static void print_constant(Value value){
    switch(value.type){
        case VAL_NONE: printf("none"); return;
        case VAL_BOOL: printf(AS_BOOL(value) ? "true" : "false"); return;
        case VAL_CHAR: printf("'%c'",AS_CHAR(value)); return;
        case VAL_NUMBER: printf("%.17g",AS_NUMBER(value)); return;
        case VAL_UNDEFINED: printf("<undefined>"); return;
        case VAL_OBJ: break;
    }

    switch(OBJ_TYPE(value)){
        case OBJ_STRING: printf("\"%.40s\"",AS_CSTRING(value)); return;
        case OBJ_FUNCTION:{
            ObjString *name = AS_FUNCTION(value)->name;
            printf("<func %s>",name != NULL ? name->chars : "fichier");
            return;
        }
        case OBJ_STRUCT: printf("<struct %s>",AS_STRUCT(value)->name->chars); return;
        case OBJ_ENUM: printf("<enum %s>",AS_ENUM(value)->name->chars); return;
        default: printf("<objet>"); return;
    }
}

static int read_u16(Chunk *chunk, int offset){
    return (chunk->code[offset] << 8) | chunk->code[offset + 1];
}

static int simple(OpCode op, int offset){
    printf("%s\n",opcode_name(op));
    return offset + 1;
}

static int byte_operand(Chunk *chunk, OpCode op, int offset){
    printf("%-18s %4d\n",opcode_name(op),chunk->code[offset + 1]);
    return offset + 2;
}

static int short_operand(Chunk *chunk, OpCode op, int offset){
    printf("%-18s %4d\n",opcode_name(op),read_u16(chunk,offset + 1));
    return offset + 3;
}

static int constant_operand(Chunk *chunk, OpCode op, int offset){
    int k = read_u16(chunk,offset + 1);
    printf("%-18s %4d  ",opcode_name(op),k);
    print_constant(chunk->constants.values[k]);
    printf("\n");
    return offset + 3;
}

// Un saut affiche aussi l'offset où il arrive.
static int jump_operand(Chunk *chunk, OpCode op, int sign, int offset){
    int distance = read_u16(chunk,offset + 1);
    printf("%-18s %4d  -> %04d\n",opcode_name(op),distance,offset + 3 + sign * distance);
    return offset + 3;
}

// Un appel par nom : le nom en constante, puis le nombre d'arguments.
static int named_call(Chunk *chunk, OpCode op, int offset){
    int k = read_u16(chunk,offset + 1);
    printf("%-18s %4d  ",opcode_name(op),k);
    print_constant(chunk->constants.values[k]);
    printf("  (%d args)\n",chunk->code[offset + 3]);
    return offset + 4;
}

static const char *type_kind_name(TypeRef *type){
    switch(type->kind){
        case TYPE_ANY: return "any";
        case TYPE_INT: return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_BOOL: return "bool";
        case TYPE_CHAR: return "char";
        case TYPE_STR: return "str";
        case TYPE_LIST: return "list";
        case TYPE_MAP: return "map";
        case TYPE_NAMED: return type->name != NULL ? type->name->chars : "?";
    }
    return "?";
}

static int check_type(Chunk *chunk, int offset){
    int c = read_u16(chunk,offset + 1);
    TypeCheck *check = &chunk->checks[c];
    static const char *kinds[] = {"variable","argument","retour"};
    printf("%-18s %4d  %s `%s` :: %s\n",opcode_name(OP_CHECK_TYPE),c,kinds[check->kind],
        check->subject != NULL ? check->subject->chars : "?",type_kind_name(&check->type));
    return offset + 3;
}

static int closure(Chunk *chunk, int offset){
    int k = read_u16(chunk,offset + 1);
    printf("%-18s %4d  ",opcode_name(OP_CLOSURE),k);
    print_constant(chunk->constants.values[k]);
    printf("\n");
    offset += 3;

    ObjFunction *function = AS_FUNCTION(chunk->constants.values[k]);
    for(int i = 0; i < function->upvalue_count; i++){
        int is_local = chunk->code[offset];
        int index = chunk->code[offset + 1];
        printf("%04d      |    %-18s %s %d\n",offset,"",is_local ? "local" : "upvalue",index);
        offset += 2;
    }
    return offset;
}

int chunk_disassemble_instruction(Chunk *chunk, int offset){
    printf("%04d ",offset);
    ast_t *node = chunk->nodes[offset];
    ast_t *previous = offset > 0 ? chunk->nodes[offset - 1] : NULL;
    if(node != NULL && previous != NULL && node->line == previous->line)
        printf("    |  ");
    else
        printf("%5d  ",node != NULL ? node->line : 0);

    OpCode op = chunk->code[offset];
    switch(op){
        case OP_CONSTANT:
        case OP_GET_BUILTIN:
        case OP_GET_FIELD:
        case OP_SET_FIELD:
        case OP_STRUCT:
        case OP_METHOD:
        case OP_ENUM:
        case OP_IMPORT:
        case OP_IMPORT_NAME:
        case OP_IMPORT_VALUE:
        case OP_EXPORT:
            return constant_operand(chunk,op,offset);

        case OP_GET_GLOBAL:
        case OP_SET_GLOBAL:
        case OP_DEFINE_GLOBAL:
        case OP_LIST:
        case OP_MAP:
            return short_operand(chunk,op,offset);

        case OP_GET_LOCAL:
        case OP_SET_LOCAL:
        case OP_GET_UPVALUE:
        case OP_SET_UPVALUE:
        case OP_CALL:
        case OP_MATCH_FAIL:
        case OP_FORMAT:
        case OP_SET_LOCAL_POP:
        case OP_GET_SELF_FIELD:
        case OP_SET_SELF_FIELD:
            return byte_operand(chunk,op,offset);

        case OP_JUMP:
        case OP_JUMP_IF_FALSE:
        case OP_SET_INDEX:
            return jump_operand(chunk,op,1,offset);
        case OP_LOOP:
            return jump_operand(chunk,op,-1,offset);

        case OP_INVOKE:
        case OP_NEW:
        case OP_TYPE_CALL:
            return named_call(chunk,op,offset);

        case OP_CHECK_TYPE:
            return check_type(chunk,offset);
        case OP_CLOSURE:
            return closure(chunk,offset);

        case OP_ITER_NEXT:{
            int distance = read_u16(chunk,offset + 2);
            printf("%-18s %4d  -> %04d\n",opcode_name(op),chunk->code[offset + 1],offset + 4 + distance);
            return offset + 4;
        }

        case OP_ADD_LOCAL:{
            int k = read_u16(chunk,offset + 2);
            printf("%-18s %4d %4d  ",opcode_name(op),chunk->code[offset + 1],k);
            print_constant(chunk->constants.values[k]);
            printf("\n");
            return offset + 4;
        }

        case OP_IMPORT_NATIVE:{
            int path = read_u16(chunk,offset + 1);
            int name = read_u16(chunk,offset + 3);
            printf("%-18s %4d %4d  ",opcode_name(op),path,name);
            print_constant(chunk->constants.values[path]);
            printf(" ");
            print_constant(chunk->constants.values[name]);
            printf("\n");
            return offset + 5;
        }

        default:
            if(op < OP_COUNT)
                return simple(op,offset);
            printf("instruction inconnue %d\n",op);
            return offset + 1;
    }
}

void chunk_disassemble(Chunk *chunk, const char *name){
    printf("== %s ==\n",name);
    for(int offset = 0; offset < chunk->count;)
        offset = chunk_disassemble_instruction(chunk,offset);
    printf("\n");
}
