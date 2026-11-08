/**
 * @file compiler.c
 * @brief traduit l'AST d'un fichier en bytecode
 *
 * [NOTE]
 *      Chaque noeud de l'AST a sa fonction compile_*, comme les visitor_visit_*
 *      de src/visitor.c. Une expression laisse sa valeur au sommet de la pile.
 *      Une instruction laisse la pile comme elle l'a trouvée : entre deux
 *      instructions, la pile ne contient que les variables locales. C'est ce
 *      qui permet de donner à chaque locale une case fixe.
 *
 *      Un nom se résout dans cet ordre, comme la chaîne des portées du visitor :
 *          les locales de la fonction
 *          dans une méthode, les attributs et les méthodes de la structure
 *          les locales des fonctions qui l'entourent (des upvalues)
 *          les globales du fichier, puis les builtins
 *
 * [HELP]
 *      use ctrl + f , '$' for walk to different part
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "natives.h"
#include "error.h"
#include "lexer.h"
#include "parser.h"

#define LOCALS_MAX 256
#define UPVALUES_MAX 256
#define TOO_DEEP "trop d'appels imbriqués : la récursion ne s'arrête peut-être jamais"


//                          ===================================== $ETAT ==============================================


// Ce qu'un nom désigne, pour refuser une redéfinition.
typedef enum{
    DEF_VARIABLE,
    DEF_FUNCTION,
    DEF_STRUCT,
    DEF_ENUM,
}DefKind;

typedef struct{
    ObjString *name;    // NULL : une case cachée (sujet d'un match, parcours d'un for ... in...)
    int depth;
    bool is_captured;   // une closure la garde : elle quitte la pile par OP_CLOSE_UPVALUE
    bool is_const;
    DefKind kind;
    TypeRef type;
}Local;

typedef struct{
    uint8_t index;
    bool is_local;      // une locale de la fonction qui entoure, sinon une de ses upvalues
}Upvalue;

// Les sauts qui attendent de connaître leur destination.
typedef struct{
    int *offsets;
    int count;
    int capacity;
}JumpList;

typedef struct Loop{
    struct Loop *enclosing;
    int break_depth;        // break retire les locales plus profondes que ça
    int continue_depth;
    int continue_target;    // -1 : continue saute en avant (corrigé plus tard)
    JumpList breaks;
    JumpList continues;
}Loop;

// Dans une méthode, la structure de self : ses attributs et ses méthodes sont visibles sans `self.`
typedef struct{
    ObjStruct *model;
    ast_t *definition;
}StructInfo;

typedef enum{
    FUNC_SCRIPT,        // le code du fichier
    FUNC_FUNCTION,
    FUNC_METHOD,        // la case 0 est self
}FuncKind;

typedef struct FuncState{
    struct FuncState *enclosing;
    ObjFunction *function;
    FuncKind kind;
    StructInfo *structure;
    Local locals[LOCALS_MAX];
    int local_count;
    Upvalue upvalues[UPVALUES_MAX];
    int scope_depth;
    Loop *loop;
}FuncState;

typedef struct{
    VM *vm;
    ObjModule *module;
    FuncState *current;
    Table file_names;       // les noms définis au niveau du fichier : ils cachent les builtins
    bool has_import_all;    // `import *` : n'importe quel nom peut devenir une globale
    DefKind *global_kinds;
    int global_kinds_count;
}Compiler;

static void compile_expr(Compiler *c, ast_t *node);
static void compile_statement(Compiler *c, ast_t *node);
static void compile_statements(Compiler *c, ast_t *node);


//                          ===================================== $EMISSION ==============================================


static Chunk *current_chunk(Compiler *c){
    return &c->current->function->chunk;
}

static void emit_byte(Compiler *c, uint8_t byte, ast_t *node){
    chunk_write(current_chunk(c),byte,node);
}

static void emit_op(Compiler *c, OpCode op, ast_t *node){
    emit_byte(c,(uint8_t)op,node);
}

static void emit_u16(Compiler *c, int value, ast_t *node){
    emit_byte(c,(uint8_t)((value >> 8) & 0xFF),node);
    emit_byte(c,(uint8_t)(value & 0xFF),node);
}

static void emit_op_u8(Compiler *c, OpCode op, int value, ast_t *node){
    emit_op(c,op,node);
    emit_byte(c,(uint8_t)value,node);
}

static void emit_op_u16(Compiler *c, OpCode op, int value, ast_t *node){
    emit_op(c,op,node);
    emit_u16(c,value,node);
}

static int make_constant(Compiler *c, Value value, ast_t *node){
    int index = chunk_add_constant(current_chunk(c),value);
    if(index > 0xFFFF)
        visitor_error(node,"trop de constantes dans une fonction (65536 au plus)");
    return index;
}

static int name_constant(Compiler *c, const char *name, ast_t *node){
    return make_constant(c,OBJ_VAL(string_from(c->vm,name)),node);
}

static void emit_constant(Compiler *c, Value value, ast_t *node){
    emit_op_u16(c,OP_CONSTANT,make_constant(c,value,node),node);
}

// Retourne la place du décalage, à corriger avec patch_jump.
static int emit_jump(Compiler *c, OpCode op, ast_t *node){
    emit_op(c,op,node);
    emit_u16(c,0xFFFF,node);
    return current_chunk(c)->count - 2;
}

// Le saut arrive ici : le décalage se compte depuis l'octet qui suit le u16.
static void patch_jump(Compiler *c, int offset, ast_t *node){
    Chunk *chunk = current_chunk(c);
    int distance = chunk->count - offset - 2;
    if(distance > 0xFFFF)
        visitor_error(node,"ce bloc est trop long : un saut dépasse 65535 octets");
    chunk->code[offset] = (uint8_t)((distance >> 8) & 0xFF);
    chunk->code[offset + 1] = (uint8_t)(distance & 0xFF);
}

static void emit_loop(Compiler *c, int start, ast_t *node){
    emit_op(c,OP_LOOP,node);
    int distance = current_chunk(c)->count - start + 2;
    if(distance > 0xFFFF)
        visitor_error(node,"cette boucle est trop longue : un saut dépasse 65535 octets");
    emit_u16(c,distance,node);
}

static void jump_list_add(JumpList *list, int offset){
    if(list->count == list->capacity){
        list->capacity = list->capacity < 8 ? 8 : list->capacity * 2;
        list->offsets = ceflo_realloc(list->offsets,sizeof(int) * (size_t)list->capacity);
    }
    list->offsets[list->count++] = offset;
}

static void jump_list_patch(Compiler *c, JumpList *list, ast_t *node){
    for(int i = 0; i < list->count; i++)
        patch_jump(c,list->offsets[i],node);
    free(list->offsets);
    *list = (JumpList){0};
}


//                          ===================================== $TYPES ==============================================


static TypeRef make_type(Compiler *c, const char *type_name){
    if(type_name == NULL)
        return (TypeRef){TYPE_ANY,NULL};
    return (TypeRef){typer_kind(type_name),string_from(c->vm,type_name)};
}

// OP_CHECK_TYPE, si la variable a un type. Une même vérification n'est rangée qu'une fois.
static void emit_check(Compiler *c, CheckKind kind, TypeRef type, ObjString *subject, ast_t *node){
    if(type.kind == TYPE_ANY)
        return;

    Chunk *chunk = current_chunk(c);
    int index = -1;
    for(int i = 0; i < chunk->check_count && index < 0; i++){
        TypeCheck *check = &chunk->checks[i];
        if(check->kind == kind && check->type.name == type.name && check->subject == subject)
            index = i;
    }
    if(index < 0)
        index = chunk_add_check(chunk,(TypeCheck){kind,type,subject});
    if(index > 0xFFFF)
        visitor_error(node,"trop de vérifications de type dans une fonction");

    emit_op_u16(c,OP_CHECK_TYPE,index,node);
}

// Une variable typée garde son type, même si on la déclare une deuxième fois.
static TypeRef merge_type(ast_t *node, ObjString *name, TypeRef old, TypeRef type){
    if(old.kind == TYPE_ANY)
        return type;
    if(type.kind != TYPE_ANY && strcmp(type.name->chars,old.name->chars) != 0)
        visitor_error(node,"`%s` est déjà de type %s",name->chars,old.name->chars);
    return old;
}

// La valeur d'une variable déclarée sans `=` : 0, false, "", [], {}... ou none.
static void emit_default(Compiler *c, TypeRef type, ast_t *node){
    switch(type.kind){
        case TYPE_INT:
        case TYPE_FLOAT: emit_constant(c,NUMBER_VAL(0),node); break;
        case TYPE_BOOL: emit_op(c,OP_FALSE,node); break;
        case TYPE_CHAR: emit_constant(c,CHAR_VAL('\0'),node); break;
        case TYPE_STR: emit_constant(c,OBJ_VAL(string_copy(c->vm,"",0)),node); break;
        case TYPE_LIST: emit_op_u16(c,OP_LIST,0,node); break;
        case TYPE_MAP: emit_op_u16(c,OP_MAP,0,node); break;
        default: emit_op(c,OP_NONE,node); break;
    }
}


//                          ===================================== $PORTEES ==============================================


static void func_state_begin(Compiler *c, FuncState *fs, FuncKind kind, ast_t *definition){
    fs->enclosing = c->current;
    fs->function = function_new(c->vm,c->module);
    fs->function->definition = definition;
    fs->kind = kind;
    fs->structure = NULL;
    fs->local_count = 0;
    fs->scope_depth = 0;
    fs->loop = NULL;
    c->current = fs;

    // La case 0 : self dans une méthode, la fonction appelée sinon.
    Local *slot = &fs->locals[fs->local_count++];
    *slot = (Local){.name = kind == FUNC_METHOD ? string_from(c->vm,"self") : NULL,.is_const = true};
    slot->type = (TypeRef){TYPE_ANY,NULL};
}

static ObjFunction *func_state_end(Compiler *c){
    ObjFunction *function = c->current->function;
    c->current = c->current->enclosing;
    return function;
}

static bool is_global_scope(Compiler *c){
    return c->current->kind == FUNC_SCRIPT && c->current->scope_depth == 0;
}

static int add_local(Compiler *c, ObjString *name, ast_t *node){
    FuncState *fs = c->current;
    if(fs->local_count == LOCALS_MAX)
        visitor_error(node,"trop de variables locales dans une fonction (%d au plus)",LOCALS_MAX);

    Local *local = &fs->locals[fs->local_count];
    *local = (Local){.name = name,.depth = fs->scope_depth,.kind = DEF_VARIABLE};
    local->type = (TypeRef){TYPE_ANY,NULL};
    return fs->local_count++;
}

// Une locale qui quitte la pile : si une closure la garde, elle part dans son upvalue.
static void emit_pop_local(Compiler *c, Local *local, ast_t *node){
    emit_op(c,local->is_captured ? OP_CLOSE_UPVALUE : OP_POP,node);
}

static void begin_scope(Compiler *c){
    c->current->scope_depth += 1;
}

static void end_scope(Compiler *c, ast_t *node){
    FuncState *fs = c->current;
    fs->scope_depth -= 1;
    while(fs->local_count > 0 && fs->locals[fs->local_count - 1].depth > fs->scope_depth){
        emit_pop_local(c,&fs->locals[fs->local_count - 1],node);
        fs->local_count -= 1;
    }
}

// break et continue : retire de la pile les locales des blocs quittés, sans les oublier.
static void emit_leave(Compiler *c, int depth, ast_t *node){
    FuncState *fs = c->current;
    for(int i = fs->local_count - 1; i >= 0 && fs->locals[i].depth > depth; i--)
        emit_pop_local(c,&fs->locals[i],node);
}

static int find_local(FuncState *fs, ObjString *name){
    for(int i = fs->local_count - 1; i >= 0; i--){
        if(fs->locals[i].name == name)
            return i;
    }
    return -1;
}

// Une locale de la portée courante seulement : `var a` deux fois réutilise la même case.
static int find_local_in_scope(FuncState *fs, ObjString *name){
    for(int i = fs->local_count - 1; i >= 0 && fs->locals[i].depth == fs->scope_depth; i--){
        if(fs->locals[i].name == name)
            return i;
    }
    return -1;
}

static int global_index(Compiler *c, ObjString *name, ast_t *node){
    int index = module_global_index(c->vm,c->module,name);
    if(index > 0xFFFF)
        visitor_error(node,"trop de globales dans un fichier (65536 au plus)");

    if(index >= c->global_kinds_count){
        int count = index + 16;
        c->global_kinds = ceflo_realloc(c->global_kinds,sizeof(DefKind) * (size_t)count);
        for(int i = c->global_kinds_count; i < count; i++)
            c->global_kinds[i] = DEF_VARIABLE;
        c->global_kinds_count = count;
    }
    return index;
}

static void add_file_name(Compiler *c, const char *name){
    table_set(&c->file_names,string_from(c->vm,name),BOOL_VAL(true));
}

// Les noms définis directement dans le fichier, même plus bas : ils cachent les builtins.
static void scan_file_names(Compiler *c, ast_t *program){
    for(int i = 0; i < program->list_compound->Size; i++){
        ast_t *node = List_Get(program->list_compound,i);
        switch(node->type){
            case AST_VARIABLE_DEFINITION:
                for(ast_t *var = node; var != NULL; var = var->left)
                    add_file_name(c,var->variable_definition_name);
                break;
            case AST_FUNCTION_DEFINITION: add_file_name(c,node->function_definition_name); break;
            case AST_STRUCT_DEFINITION:
            case AST_ENUM_DEFINITION: add_file_name(c,node->struct_definition_name); break;
            case AST_INCLUDE:
                if(node->is_import_all){
                    c->has_import_all = true;
                }else{
                    for(int j = 0; j < node->list_compound->Size; j++)
                        add_file_name(c,((ast_t*)List_Get(node->list_compound,j))->variable_name);
                }
                break;
            default: break;
        }
    }
}


//                          ===================================== $NOMS ==============================================


typedef enum{
    NAME_LOCAL,
    NAME_UPVALUE,
    NAME_FIELD,         // dans une méthode : self.nom
    NAME_METHOD,        // dans une méthode : self.nom(...)
    NAME_GLOBAL,
    NAME_BUILTIN,
}NameKind;

typedef struct{
    NameKind kind;
    int index;              // la case, l'upvalue, la globale, ou la constante du nom d'un builtin
    NameKind self_kind;     // NAME_FIELD, NAME_METHOD : self est une locale ou une upvalue
    int self_index;
    ObjString *name;
    bool is_const;
    TypeRef type;
}Name;

static bool struct_has_method(StructInfo *info, ObjString *name){
    list_t *items = info->definition->struct_definition_item;
    for(int i = 0; i < items->Size; i++){
        ast_t *item = List_Get(items,i);
        if(item->type == AST_FUNCTION_DEFINITION && strcmp(item->function_definition_name,name->chars) == 0 &&
           strcmp(name->chars,"__init__") != 0 && strcmp(name->chars,"__cast__") != 0){
            return true;
        }
    }
    return false;
}

static int add_upvalue(FuncState *fs, int index, bool is_local, ast_t *node){
    int count = fs->function->upvalue_count;
    for(int i = 0; i < count; i++){
        if(fs->upvalues[i].index == index && fs->upvalues[i].is_local == is_local)
            return i;
    }
    if(count == UPVALUES_MAX)
        visitor_error(node,"une fonction capture trop de variables (%d au plus)",UPVALUES_MAX);

    fs->upvalues[count] = (Upvalue){(uint8_t)index,is_local};
    return fs->function->upvalue_count++;
}

// Cherche le nom dans les fonctions, de la plus proche à la plus lointaine.
// Retourne false s'il n'y est pas : c'est une globale ou un builtin.
static bool resolve_in_function(FuncState *fs, ObjString *name, Name *out, ast_t *node){
    int slot = find_local(fs,name);
    if(slot >= 0){
        out->kind = NAME_LOCAL;
        out->index = slot;
        out->is_const = fs->locals[slot].is_const;
        out->type = fs->locals[slot].type;
        return true;
    }

    if(fs->structure != NULL){
        ObjStruct *model = fs->structure->model;
        int field = struct_field_index(model,name);
        if(field >= 0 || struct_has_method(fs->structure,name)){
            out->kind = field >= 0 ? NAME_FIELD : NAME_METHOD;
            out->index = field;
            out->self_kind = NAME_LOCAL;
            out->self_index = 0;
            out->is_const = field >= 0 && model->field_is_const[field];
            out->type = field >= 0 ? model->field_types[field] : (TypeRef){TYPE_ANY,NULL};
            return true;
        }
    }

    if(fs->enclosing == NULL || !resolve_in_function(fs->enclosing,name,out,node))
        return false;

    // Trouvé plus loin : la fonction le capture.
    FuncState *outer = fs->enclosing;
    if(out->kind == NAME_LOCAL){
        outer->locals[out->index].is_captured = true;
        out->index = add_upvalue(fs,out->index,true,node);
        out->kind = NAME_UPVALUE;
    }else if(out->kind == NAME_UPVALUE){
        out->index = add_upvalue(fs,out->index,false,node);
    }else{
        if(out->self_kind == NAME_LOCAL)
            outer->locals[out->self_index].is_captured = true;
        out->self_index = add_upvalue(fs,out->self_index,out->self_kind == NAME_LOCAL,node);
        out->self_kind = NAME_UPVALUE;
    }
    return true;
}

static Name resolve_name(Compiler *c, const char *chars, ast_t *node){
    Name result = {.name = string_from(c->vm,chars)};
    result.type = (TypeRef){TYPE_ANY,NULL};

    if(resolve_in_function(c->current,result.name,&result,node))
        return result;

    Value unused;
    if(!c->has_import_all && !table_get(&c->file_names,result.name,&unused) &&
       table_get(&c->vm->builtins,result.name,&unused)){
        result.kind = NAME_BUILTIN;
        result.index = make_constant(c,OBJ_VAL(result.name),node);
        return result;
    }

    // Un nom inconnu devient une globale : la VM dira qu'elle n'est pas définie.
    result.kind = NAME_GLOBAL;
    result.index = global_index(c,result.name,node);
    result.is_const = c->module->is_const[result.index];
    result.type = c->module->types[result.index];
    return result;
}

static void emit_get_self(Compiler *c, Name *name, ast_t *node){
    emit_op_u8(c,name->self_kind == NAME_LOCAL ? OP_GET_LOCAL : OP_GET_UPVALUE,name->self_index,node);
}

static void emit_get_name(Compiler *c, Name *name, ast_t *node){
    switch(name->kind){
        case NAME_LOCAL: emit_op_u8(c,OP_GET_LOCAL,name->index,node); break;
        case NAME_UPVALUE: emit_op_u8(c,OP_GET_UPVALUE,name->index,node); break;
        case NAME_GLOBAL: emit_op_u16(c,OP_GET_GLOBAL,name->index,node); break;
        case NAME_BUILTIN: emit_op_u16(c,OP_GET_BUILTIN,name->index,node); break;
        case NAME_FIELD:
            if(name->self_kind == NAME_LOCAL && name->index < 256){
                emit_op_u8(c,OP_GET_SELF_FIELD,name->index,node);
                break;
            }
            // fallthrough
        case NAME_METHOD:
            emit_get_self(c,name,node);
            emit_op_u16(c,OP_GET_FIELD,make_constant(c,OBJ_VAL(name->name),node),node);
            break;
    }
}

// Range la valeur du sommet de la pile et la retire. Pas pour NAME_FIELD : self doit être dessous.
static void emit_store_name(Compiler *c, Name *name, ast_t *node){
    switch(name->kind){
        case NAME_LOCAL: emit_op_u8(c,OP_SET_LOCAL_POP,name->index,node); return;
        case NAME_UPVALUE: emit_op_u8(c,OP_SET_UPVALUE,name->index,node); break;
        default: emit_op_u16(c,OP_SET_GLOBAL,name->index,node); break;
    }
    emit_op(c,OP_POP,node);
}

/**
 * @brief dans une méthode, la case de l'attribut que désigne `target` (`x` ou
 *        `self.x`), ou -1. self est alors slots[0] : OP_GET_SELF_FIELD lit la case
 *        sans chercher le nom.
 */
static int self_field_slot(Compiler *c, ast_t *target, Name *name){
    int slot = -1;
    if(target->type == AST_VARIABLE){
        if(name->kind == NAME_FIELD && name->self_kind == NAME_LOCAL)
            slot = name->index;
    }else if(target->type == AST_OBJ_ACCESS && target->left->type == AST_VARIABLE &&
             strcmp(target->left->variable_name,"self") == 0){
        FuncState *fs = c->current;
        if(fs->kind == FUNC_METHOD && fs->structure != NULL && find_local(fs,string_from(c->vm,"self")) == 0)
            slot = struct_field_index(fs->structure->model,string_from(c->vm,target->variable_name));
    }
    return slot < 256 ? slot : -1;
}


//                          ===================================== $DEFINITIONS ==============================================


static DefKind *scope_kind(Compiler *c, ObjString *name, ast_t *node){
    if(is_global_scope(c)){
        int index = global_index(c,name,node);
        return &c->global_kinds[index];
    }
    int slot = find_local_in_scope(c->current,name);
    return slot >= 0 ? &c->current->locals[slot].kind : NULL;
}

// La valeur au sommet de la pile devient la variable `name` de la portée courante.
static void define_top(Compiler *c, ObjString *name, DefKind kind, bool is_const, ast_t *node){
    if(is_global_scope(c)){
        int index = global_index(c,name,node);
        c->global_kinds[index] = kind;
        c->module->is_const[index] = is_const;
        emit_op_u16(c,OP_DEFINE_GLOBAL,index,node);
        return;
    }

    int slot = find_local_in_scope(c->current,name);
    if(slot < 0)
        slot = add_local(c,name,node);
    else
        emit_op_u8(c,OP_SET_LOCAL_POP,slot,node);
    c->current->locals[slot].kind = kind;
    c->current->locals[slot].is_const = is_const;
}

// Une fonction ou une structure locale se voit elle-même : sa case existe avant son code.
// Retourne false si la case n'est pas encore créée (une globale, ou une redéfinition).
static bool declare_early(Compiler *c, ObjString *name, DefKind kind, ast_t *node){
    if(is_global_scope(c) || find_local_in_scope(c->current,name) >= 0)
        return false;
    int slot = add_local(c,name,node);
    c->current->locals[slot].kind = kind;
    return true;
}

static void compile_definition_value(Compiler *c, ast_t *node, TypeRef type, ObjString *name){
    if(node->variable_definition_value != NULL)
        compile_expr(c,node->variable_definition_value);
    else
        emit_default(c,type,node);
    emit_check(c,CHECK_VARIABLE,type,name,node);
}

static void compile_one_definition(Compiler *c, ast_t *node){
    ObjString *name = string_from(c->vm,node->variable_definition_name);
    TypeRef type = make_type(c,node->variable_definition_static_type_name);

    if(is_global_scope(c)){
        int index = global_index(c,name,node);
        type = merge_type(node,name,c->module->types[index],type);
        compile_definition_value(c,node,type,name);
        c->module->types[index] = type;
        c->module->is_const[index] = node->is_const;
        c->global_kinds[index] = DEF_VARIABLE;
        emit_op_u16(c,OP_DEFINE_GLOBAL,index,node);
        return;
    }

    FuncState *fs = c->current;
    int slot = find_local_in_scope(fs,name);
    if(slot >= 0){
        type = merge_type(node,name,fs->locals[slot].type,type);
        compile_definition_value(c,node,type,name);
        emit_op_u8(c,OP_SET_LOCAL_POP,slot,node);
    }else{
        // La valeur se calcule avant que le nom existe : `var x = x + 1` lit l'ancien x.
        compile_definition_value(c,node,type,name);
        slot = add_local(c,name,node);
    }
    fs->locals[slot].type = type;
    fs->locals[slot].is_const = node->is_const;
    fs->locals[slot].kind = DEF_VARIABLE;
}

// var a = 1, b::int = 2
static void compile_variable_definition(Compiler *c, ast_t *node){
    for(; node != NULL; node = node->left)
        compile_one_definition(c,node);
}


//                          ===================================== $FONCTIONS ==============================================


static void emit_return(Compiler *c, ast_t *node){
    ObjFunction *function = c->current->function;
    emit_check(c,CHECK_RETURN,function->return_type,function->name,node);
    emit_op(c,OP_RETURN,node);
}

// Ferme la fonction en cours et pose sa closure dans la fonction qui l'entoure.
static ObjFunction *emit_closure(Compiler *c, ast_t *node){
    FuncState *fs = c->current;
    ObjFunction *function = func_state_end(c);

    emit_op_u16(c,OP_CLOSURE,make_constant(c,OBJ_VAL(function),node),node);
    for(int i = 0; i < function->upvalue_count; i++){
        emit_byte(c,fs->upvalues[i].is_local ? 1 : 0,node);
        emit_byte(c,fs->upvalues[i].index,node);
    }
    return function;
}

/**
 * @brief le corps d'une fonction partage la portée de ses paramètres : `var x`
 *        y réutilise la case du paramètre x, comme dans le visitor.
 */
static void compile_function(Compiler *c, ast_t *node, FuncKind kind, StructInfo *structure){
    FuncState fs;
    func_state_begin(c,&fs,kind,node);
    fs.structure = structure;

    ObjFunction *function = fs.function;
    list_t *args = node->function_definition_args;
    function->name = string_from(c->vm,node->function_definition_name);
    function->arity = args->Size;
    function->is_method = kind == FUNC_METHOD;
    function->return_type = make_type(c,node->function_definition_return_type_name);
    if(args->Size > 0)
        function->param_types = ceflo_alloc(sizeof(TypeRef) * (size_t)args->Size);

    begin_scope(c);
    for(int i = 0; i < args->Size; i++){
        ast_t *arg = List_Get(args,i);
        int slot = add_local(c,string_from(c->vm,arg->variable_name),arg);
        fs.locals[slot].type = make_type(c,arg->variable_definition_static_type_name);
        function->param_types[i] = fs.locals[slot].type;
    }
    for(int i = 0; i < args->Size; i++){
        ast_t *arg = List_Get(args,i);
        if(fs.locals[i + 1].type.kind == TYPE_ANY)
            continue;
        emit_op_u8(c,OP_GET_LOCAL,i + 1,arg);
        emit_check(c,CHECK_ARGUMENT,fs.locals[i + 1].type,fs.locals[i + 1].name,arg);
        emit_op(c,OP_POP,arg);
    }

    ast_t *body = node->function_definition_body;
    compile_statements(c,body);

    list_t *statements = body->list_compound;
    bool ends_with_return = statements->Size > 0 &&
        ((ast_t*)List_Get(statements,statements->Size - 1))->type == AST_RETURN;
    if(!ends_with_return){
        emit_op(c,OP_NONE,node);
        emit_return(c,node);
    }

    emit_closure(c,node);
}

static void compile_function_definition(Compiler *c, ast_t *node){
    ObjString *name = string_from(c->vm,node->function_definition_name);
    DefKind *kind = scope_kind(c,name,node);
    if(kind != NULL && *kind == DEF_FUNCTION)
        visitor_error(node,"la fonction `%s` est déjà définie",name->chars);

    if(declare_early(c,name,DEF_FUNCTION,node)){
        compile_function(c,node,FUNC_FUNCTION,NULL);
        return;
    }
    compile_function(c,node,FUNC_FUNCTION,NULL);
    define_top(c,name,DEF_FUNCTION,false,node);
}

static void compile_return(Compiler *c, ast_t *node){
    if(node->return_expr != NULL)
        compile_expr(c,node->return_expr);
    else
        emit_op(c,OP_NONE,node);
    emit_return(c,node);
}


//                          ===================================== $STRUCTURES ==============================================


// Une structure et une énumération ne se redéfinissent pas.
static void check_new_type(Compiler *c, ast_t *node){
    DefKind *kind = scope_kind(c,string_from(c->vm,node->struct_definition_name),node);
    if(kind != NULL && *kind == DEF_STRUCT)
        visitor_error(node,"la structure `%s` est déjà définie",node->struct_definition_name);
    if(kind != NULL && *kind == DEF_ENUM)
        visitor_error(node,"l'énumération `%s` est déjà définie",node->struct_definition_name);
}

// Le modèle de la structure : son nom, et le nom, le type et la constance de chaque attribut.
static ObjStruct *build_struct_model(Compiler *c, ast_t *node){
    list_t *items = node->struct_definition_item;
    ObjString *names[LOCALS_MAX];
    int count = 0;

    for(int i = 0; i < items->Size; i++){
        for(ast_t *var = List_Get(items,i); var != NULL && var->type == AST_VARIABLE_DEFINITION; var = var->left){
            ObjString *name = string_from(c->vm,var->variable_definition_name);
            bool known = false;
            for(int j = 0; j < count; j++)
                known = known || names[j] == name;
            if(known)
                continue;
            if(count == LOCALS_MAX)
                visitor_error(var,"une structure a au plus %d attributs",LOCALS_MAX);
            names[count++] = name;
        }
    }

    ObjStruct *model = struct_new(c->vm,string_from(c->vm,node->struct_definition_name),node,count);
    for(int i = 0; i < count; i++)
        model->field_names[i] = names[i];

    for(int i = 0; i < items->Size; i++){
        for(ast_t *var = List_Get(items,i); var != NULL && var->type == AST_VARIABLE_DEFINITION; var = var->left){
            int field = struct_field_index(model,string_from(c->vm,var->variable_definition_name));
            TypeRef type = make_type(c,var->variable_definition_static_type_name);
            model->field_types[field] = merge_type(var,model->field_names[field],model->field_types[field],type);
            model->field_is_const[field] = var->is_const;
        }
    }
    return model;
}

/**
 * @brief la closure qui donne leur valeur de départ aux attributs, dans l'ordre,
 *        comme visitor_new_object. Un attribut voit self et les attributs d'avant.
 */
static void compile_fields_init(Compiler *c, ast_t *node, StructInfo *info){
    FuncState fs;
    func_state_begin(c,&fs,FUNC_METHOD,node);
    fs.structure = info;
    fs.function->name = info->model->name;
    fs.function->is_method = true;
    begin_scope(c);

    list_t *items = node->struct_definition_item;
    for(int i = 0; i < items->Size; i++){
        for(ast_t *var = List_Get(items,i); var != NULL && var->type == AST_VARIABLE_DEFINITION; var = var->left){
            ObjString *name = string_from(c->vm,var->variable_definition_name);
            emit_op_u8(c,OP_GET_LOCAL,0,var);
            if(var->variable_definition_value != NULL)
                compile_expr(c,var->variable_definition_value);
            else
                emit_default(c,info->model->field_types[struct_field_index(info->model,name)],var);
            emit_op_u16(c,OP_SET_FIELD,make_constant(c,OBJ_VAL(name),var),var);
            emit_op(c,OP_POP,var);
        }
    }

    emit_op(c,OP_NONE,node);
    emit_op(c,OP_RETURN,node);
    emit_closure(c,node);
}

/**
 * @brief OP_STRUCT pose une copie du modèle, chaque OP_METHOD y range une méthode,
 *        puis OP_FIELDS_INIT la closure des attributs.
 *        __cast__ n'a pas de self : c'est une fonction de la portée de la structure.
 */
static void compile_struct_definition(Compiler *c, ast_t *node){
    check_new_type(c,node);

    ObjString *name = string_from(c->vm,node->struct_definition_name);
    StructInfo info = {build_struct_model(c,node),node};
    bool declared = declare_early(c,name,DEF_STRUCT,node);

    emit_op_u16(c,OP_STRUCT,make_constant(c,OBJ_VAL(info.model),node),node);

    list_t *items = node->struct_definition_item;
    for(int i = 0; i < items->Size; i++){
        ast_t *item = List_Get(items,i);
        if(item->type != AST_FUNCTION_DEFINITION)
            continue;

        bool is_cast = strcmp(item->function_definition_name,"__cast__") == 0;
        if(strcmp(item->function_definition_name,"__init__") == 0 && item->function_definition_args->Size > 7)
            visitor_error(item,"`__init__` prend au plus 7 paramètres");

        compile_function(c,item,is_cast ? FUNC_FUNCTION : FUNC_METHOD,is_cast ? NULL : &info);
        emit_op_u16(c,OP_METHOD,name_constant(c,item->function_definition_name,item),item);
    }

    if(info.model->field_count > 0){
        compile_fields_init(c,node,&info);
        emit_op(c,OP_FIELDS_INIT,node);
    }

    if(!declared)
        define_top(c,name,DEF_STRUCT,false,node);
}

// enum Couleur { ROUGE, VERT = 5, BLEU = "b" } : le modèle contient déjà chaque élément.
static void compile_enum_definition(Compiler *c, ast_t *node){
    check_new_type(c,node);

    list_t *items = node->struct_definition_item;
    ObjString *name = string_from(c->vm,node->struct_definition_name);
    ObjEnum *model = enum_new(c->vm,name,node,items->Size);

    for(int i = 0; i < items->Size; i++){
        ast_t *item = List_Get(items,i);
        Value value = item->type == AST_NUMBER ? NUMBER_VAL(item->number_value) :
            OBJ_VAL(string_copy(c->vm,item->string_value,item->string_lenght));
        model->items[i] = enum_value_new(c->vm,model,string_from(c->vm,item->variable_name),value);
    }

    emit_op_u16(c,OP_ENUM,make_constant(c,OBJ_VAL(model),node),node);
    define_top(c,name,DEF_ENUM,true,node);
}


//                          ===================================== $VALEURS ==============================================


// La valeur d'un nombre, d'un caractère ou d'une chaîne écrits dans le code.
static Value constant_of(Compiler *c, ast_t *node){
    switch(node->type){
        case AST_NUMBER: return NUMBER_VAL(node->number_value);
        case AST_CHAR: return CHAR_VAL(node->char_value);
        default: return OBJ_VAL(string_copy(c->vm,node->string_value,node->string_lenght));
    }
}

static void compile_list(Compiler *c, ast_t *node){
    list_t *items = node->list_compound;
    if(items->Size > 0xFFFF)
        visitor_error(node,"une liste écrite dans le code a au plus 65535 éléments");
    for(int i = 0; i < items->Size; i++)
        compile_expr(c,List_Get(items,i));
    emit_op_u16(c,OP_LIST,items->Size,node);
}

static void compile_map(Compiler *c, ast_t *node){
    list_t *items = node->list_compound;
    if(items->Size / 2 > 0xFFFF)
        visitor_error(node,"un map écrit dans le code a au plus 65535 clés");
    for(int i = 0; i < items->Size; i++)
        compile_expr(c,List_Get(items,i));
    emit_op_u16(c,OP_MAP,items->Size / 2,node);
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

// `and` et `or` n'évaluent leur côté droit que s'il est utile, et donnent un bool.
static void compile_logical(Compiler *c, ast_t *node){
    compile_expr(c,node->left);
    int skip = emit_jump(c,OP_JUMP_IF_FALSE,node);

    if(node->op == TOKEN_AND){
        compile_expr(c,node->right);
        emit_op(c,OP_TO_BOOL,node);
        int end = emit_jump(c,OP_JUMP,node);
        patch_jump(c,skip,node);
        emit_op(c,OP_FALSE,node);
        patch_jump(c,end,node);
    }else{
        emit_op(c,OP_TRUE,node);
        int end = emit_jump(c,OP_JUMP,node);
        patch_jump(c,skip,node);
        compile_expr(c,node->right);
        emit_op(c,OP_TO_BOOL,node);
        patch_jump(c,end,node);
    }
}

static void compile_binop(Compiler *c, ast_t *node){
    if(node->op == TOKEN_AND || node->op == TOKEN_OR){
        compile_logical(c,node);
        return;
    }
    compile_expr(c,node->left);
    compile_expr(c,node->right);
    emit_op(c,binary_op(node->op),node);
}

static void compile_unop(Compiler *c, ast_t *node){
    if(node->op == TOKEN_MINUS && node->right->type == AST_NUMBER){
        emit_constant(c,NUMBER_VAL(-node->right->number_value),node);
        return;
    }
    compile_expr(c,node->right);
    emit_op(c,node->op == TOKEN_NOT ? OP_NOT : OP_NEGATE,node);
}

/**
 * @brief une condition d'un if, d'un while... : chaque saut ajouté à `false_jumps`
 *        part quand elle est fausse. `a and b` n'a pas besoin de construire de bool.
 */
static void compile_condition(Compiler *c, ast_t *node, JumpList *false_jumps){
    if(node->type == AST_BINOP && node->op == TOKEN_AND){
        compile_condition(c,node->left,false_jumps);
        compile_condition(c,node->right,false_jumps);
        return;
    }
    compile_expr(c,node);
    jump_list_add(false_jumps,emit_jump(c,OP_JUMP_IF_FALSE,node));
}

static void compile_ternary(Compiler *c, ast_t *node){
    JumpList false_jumps = {0};
    compile_condition(c,node->if_expr,&false_jumps);
    compile_expr(c,node->if_other);
    int end = emit_jump(c,OP_JUMP,node);
    jump_list_patch(c,&false_jumps,node);
    compile_expr(c,node->else_body);
    patch_jump(c,end,node);
}


//                          ===================================== $APPELS ==============================================


static void compile_args(Compiler *c, ast_t *node){
    list_t *args = node->function_call_args;
    if(args->Size > 255)
        visitor_error(node,"un appel prend au plus 255 arguments");
    for(int i = 0; i < args->Size; i++)
        compile_expr(c,List_Get(args,i));
}

// o.nom(args) : l'opcode et n portent l'appel, le nom porte `o.nom` (pour « n'a pas de méthode »).
static void emit_invoke(Compiler *c, ast_t *node, ObjString *name, ast_t *callee){
    emit_op(c,OP_INVOKE,node);
    emit_u16(c,make_constant(c,OBJ_VAL(name),callee),callee);
    emit_byte(c,(uint8_t)node->function_call_args->Size,node);
}

/**
 * @brief format("Bonjour ${nom}") : chaque ${...} est compilé dans la portée de
 *        l'appel, et OP_FORMAT met les morceaux bout à bout. \${ écrit un vrai ${.
 */
static void compile_format(Compiler *c, ast_t *node, ast_t *text){
    const char *format = text->string_value;
    int length = text->string_lenght;
    char *literal = ceflo_alloc((size_t)length + 1);
    int literal_length = 0;
    int parts = 0;
    bool has_expr = false;

    for(int i = 0; i <= length; i++){
        bool at_expr = i + 1 < length && format[i] == '$' && format[i + 1] == '{';

        // Le texte accumulé devient une constante avant une expression, et à la fin.
        if((at_expr || i == length) && literal_length > 0){
            emit_constant(c,OBJ_VAL(string_copy(c->vm,literal,literal_length)),text);
            literal_length = 0;
            parts += 1;
        }
        if(parts == 255){
            emit_op_u8(c,OP_FORMAT,parts,node);
            parts = 1;
        }
        if(i == length)
            break;

        if(format[i] == '\\' && i + 1 < length && format[i + 1] == '$'){
            literal[literal_length++] = '$';
            i += 1;
        }else if(at_expr){
            int start = i + 2;
            int depth = 1;
            int end = start;
            for(; end < length; end++){
                if(format[end] == '{')
                    depth += 1;
                if(format[end] == '}' && --depth == 0)
                    break;
            }
            if(depth != 0)
                visitor_error(node,"`format` : ${ n'est jamais fermé");

            char *source = ceflo_alloc((size_t)(end - start) + 1);
            memcpy(source,format + start,(size_t)(end - start));
            source[end - start] = '\0';
            lexer_t lexer;
            newLexerWithString(&lexer,"format",source);
            compile_expr(c,parser_parse_one(&lexer));
            free(source);

            parts += 1;
            has_expr = true;
            i = end;
        }else{
            literal[literal_length++] = format[i];
        }
    }
    free(literal);

    if(parts == 0)
        emit_constant(c,OBJ_VAL(string_copy(c->vm,"",0)),text);
    else if(has_expr || parts > 1)
        emit_op_u8(c,OP_FORMAT,parts,node);
}

static bool is_format_call(Name *name, ast_t *node){
    if(name->kind != NAME_BUILTIN || node->function_call_args->Size != 1 ||
       ((ast_t*)List_Get(node->function_call_args,0))->type != AST_STRING){
        return false;
    }
    return strcmp(name->name->chars,"format") == 0 || strcmp(name->name->chars,"printf") == 0;
}

static void compile_call(Compiler *c, ast_t *node){
    ast_t *callee = node->left;

    if(callee->type == AST_OBJ_ACCESS){
        compile_expr(c,callee->left);
        compile_args(c,node);
        emit_invoke(c,node,string_from(c->vm,callee->variable_name),callee);
        return;
    }

    if(callee->type == AST_VARIABLE){
        Name name = resolve_name(c,callee->variable_name,callee);

        if(is_format_call(&name,node)){
            bool is_printf = strcmp(name.name->chars,"printf") == 0;
            if(is_printf)
                emit_op_u16(c,OP_GET_BUILTIN,name_constant(c,"print",callee),callee);
            compile_format(c,node,List_Get(node->function_call_args,0));
            if(is_printf)
                emit_op_u8(c,OP_CALL,1,node);
            return;
        }

        // Dans une méthode, `m()` veut dire `self.m()`.
        if(name.kind == NAME_METHOD){
            emit_get_self(c,&name,callee);
            compile_args(c,node);
            emit_invoke(c,node,name.name,callee);
            return;
        }
        emit_get_name(c,&name,callee);
    }else{
        compile_expr(c,callee);
    }

    compile_args(c,node);
    emit_op_u8(c,OP_CALL,node->function_call_args->Size,node);
}

// new Nom(args) : une structure globale est poussée par son nom, que OP_NEW cherche lui-même.
static void compile_new(Compiler *c, ast_t *node){
    Name name = resolve_name(c,node->function_call_name,node);
    if(name.kind == NAME_GLOBAL || name.kind == NAME_BUILTIN)
        emit_constant(c,OBJ_VAL(name.name),node);
    else
        emit_get_name(c,&name,node);

    compile_args(c,node);
    emit_op_u16(c,OP_NEW,make_constant(c,OBJ_VAL(name.name),node),node);
    emit_byte(c,(uint8_t)node->function_call_args->Size,node);
}

static void compile_type_call(Compiler *c, ast_t *node){
    compile_args(c,node);
    emit_op_u16(c,OP_TYPE_CALL,name_constant(c,node->function_call_name,node),node);
    emit_byte(c,(uint8_t)node->function_call_args->Size,node);
}


//                          ===================================== $AFFECTATIONS ==============================================


static void compile_set_index(Compiler *c, ast_t *target);

// La nouvelle valeur, ou pour `+=` l'opération entre la valeur actuelle (déjà sur la pile) et elle.
static void compile_assigned_value(Compiler *c, ast_t *node){
    compile_expr(c,node->assignement_value);
    if(node->op != TOKEN_EQUAL)
        emit_op(c,binary_op(node->op),node);
}

static void check_not_const(Name *name, ast_t *target){
    if(name->is_const)
        visitor_error(target,"`%s` est une constante",name->name->chars);
}

// self.x = v dans une méthode : la vérification que ferait OP_SET_FIELD est écrite ici.
static void emit_self_field_store(Compiler *c, int slot, ast_t *target){
    ObjStruct *model = c->current->structure->model;
    if(model->field_is_const[slot])
        visitor_error(target,"`%s` est une constante",model->field_names[slot]->chars);
    emit_check(c,CHECK_VARIABLE,model->field_types[slot],model->field_names[slot],target);
    emit_op_u8(c,OP_SET_SELF_FIELD,slot,target);
}

// x += 1 sur une locale sans type : une seule instruction.
static bool is_add_local(Name *name, ast_t *node){
    ast_type value = node->assignement_value->type;
    return name->kind == NAME_LOCAL && name->type.kind == TYPE_ANY && node->op == TOKEN_PLUS &&
        (value == AST_NUMBER || value == AST_STRING || value == AST_CHAR);
}

/**
 * @brief range la valeur du sommet de la pile dans `place`, puis la retire.
 *        Sert à s[i] = 'x' : la nouvelle chaîne remplace l'ancienne là où elle était.
 */
static void compile_store(Compiler *c, ast_t *place){
    if(place->type == AST_VARIABLE){
        Name name = resolve_name(c,place->variable_name,place);
        if(name.kind != NAME_FIELD && name.kind != NAME_METHOD){
            if(name.kind == NAME_BUILTIN){
                name.kind = NAME_GLOBAL;
                name.index = global_index(c,name.name,place);
            }
            check_not_const(&name,place);
            if(name.kind != NAME_GLOBAL)
                emit_check(c,CHECK_VARIABLE,name.type,name.name,place);
            emit_store_name(c,&name,place);
            return;
        }
    }else if(place->type != AST_OBJ_ACCESS && place->type != AST_LIST_ACCESS){
        emit_op(c,OP_POP,place);
        return;
    }

    Name self = {.kind = NAME_FIELD,.self_kind = NAME_LOCAL,.index = -1};
    if(place->type == AST_VARIABLE)
        self = resolve_name(c,place->variable_name,place);
    int field_slot = place->type == AST_LIST_ACCESS ? -1 : self_field_slot(c,place,&self);
    if(field_slot >= 0){
        emit_self_field_store(c,field_slot,place);
        return;
    }

    // Le conteneur doit passer sous la valeur : elle attend dans une case cachée.
    int temp = add_local(c,NULL,place);
    if(place->type == AST_LIST_ACCESS){
        compile_expr(c,place->left);
        compile_expr(c,place->list_ptr);
        emit_op_u8(c,OP_GET_LOCAL,temp,place);
        compile_set_index(c,place);
    }else{
        const char *field = place->variable_name;
        if(place->type == AST_OBJ_ACCESS){
            compile_expr(c,place->left);
        }else{
            Name name = resolve_name(c,field,place);
            emit_get_self(c,&name,place);
        }
        emit_op_u8(c,OP_GET_LOCAL,temp,place);
        emit_op_u16(c,OP_SET_FIELD,name_constant(c,field,place),place);
        emit_op(c,OP_POP,place);
    }
    c->current->local_count -= 1;
    emit_op(c,OP_POP,place);
}

// [c i v] -> [] ; si c est une chaîne, le code qui suit range la nouvelle chaîne.
static void compile_set_index(Compiler *c, ast_t *target){
    int jump = emit_jump(c,OP_SET_INDEX,target);
    compile_store(c,target->left);
    patch_jump(c,jump,target);
}

/*
    La cible s'évalue une seule fois : dans `l[f()] += 1`, f n'est appelée
    qu'une fois. Pour `+=`, DUP ou DUP2 garde la cible pendant la lecture.
*/
static void compile_assignement(Compiler *c, ast_t *node){
    ast_t *target = node->assignement_left;
    bool compound = node->op != TOKEN_EQUAL;

    if(target->type == AST_LIST_ACCESS){
        compile_expr(c,target->left);
        compile_expr(c,target->list_ptr);
        if(compound){
            emit_op(c,OP_DUP2,target);
            emit_op(c,OP_GET_INDEX,target);
        }
        compile_assigned_value(c,node);
        compile_set_index(c,target);
        return;
    }

    Name name = {.kind = NAME_FIELD};
    if(target->type == AST_VARIABLE){
        name = resolve_name(c,target->variable_name,target);
        check_not_const(&name,target);
    }

    int field_slot = self_field_slot(c,target,&name);
    if(field_slot >= 0){
        if(compound)
            emit_op_u8(c,OP_GET_SELF_FIELD,field_slot,target);
        compile_assigned_value(c,node);
        emit_self_field_store(c,field_slot,target);
        return;
    }

    if(name.kind == NAME_FIELD || name.kind == NAME_METHOD){
        if(target->type == AST_OBJ_ACCESS)
            compile_expr(c,target->left);
        else
            emit_get_self(c,&name,target);

        int field = name_constant(c,target->variable_name,target);
        if(compound){
            emit_op(c,OP_DUP,target);
            emit_op_u16(c,OP_GET_FIELD,field,target);
        }
        compile_assigned_value(c,node);
        emit_op_u16(c,OP_SET_FIELD,field,target);
        emit_op(c,OP_POP,node);
        return;
    }

    if(name.kind == NAME_BUILTIN){
        name.kind = NAME_GLOBAL;
        name.index = global_index(c,name.name,target);
    }
    if(is_add_local(&name,node)){
        emit_op_u8(c,OP_ADD_LOCAL,name.index,node);
        emit_u16(c,make_constant(c,constant_of(c,node->assignement_value),node),node);
        return;
    }
    if(compound)
        emit_get_name(c,&name,target);
    compile_assigned_value(c,node);
    if(name.kind != NAME_GLOBAL)
        emit_check(c,CHECK_VARIABLE,name.type,name.name,target);
    emit_store_name(c,&name,target);
}


//                          ===================================== $CONTROLE ==============================================


static void compile_compound(Compiler *c, ast_t *node){
    if(!node->has_definition){
        compile_statements(c,node);
        return;
    }
    begin_scope(c);
    compile_statements(c,node);
    end_scope(c,node);
}

static void compile_if(Compiler *c, ast_t *node){
    JumpList false_jumps = {0};
    compile_condition(c,node->if_expr,&false_jumps);
    compile_statement(c,node->if_body);

    ast_t *other = node->if_other != NULL ? node->if_other : node->else_body;
    if(other == NULL){
        jump_list_patch(c,&false_jumps,node);
        return;
    }
    int end = emit_jump(c,OP_JUMP,node);
    jump_list_patch(c,&false_jumps,node);
    compile_statement(c,other);
    patch_jump(c,end,node);
}

/**
 * @brief le sujet attend dans une case cachée. Le premier `case` égal s'exécute ;
 *        sans `else`, OP_MATCH_FAIL donne l'erreur si une énumération est en jeu.
 */
static void compile_match(Compiler *c, ast_t *node){
    begin_scope(c);
    compile_expr(c,node->if_expr);
    int subject = add_local(c,NULL,node);

    JumpList ends = {0};
    bool has_enum = false;

    for(int i = 0; i < node->list_compound->Size; i++){
        ast_t *ast_case = List_Get(node->list_compound,i);
        list_t *values = ast_case->list_compound;
        JumpList to_body = {0};
        int next_case = -1;

        for(int j = 0; j < values->Size; j++){
            ast_t *value = List_Get(values,j);
            has_enum = has_enum || value->type == AST_OBJ_ACCESS;

            emit_op_u8(c,OP_GET_LOCAL,subject,value);
            compile_expr(c,value);
            if(j < values->Size - 1){
                emit_op(c,OP_NOT_EQUAL,value);
                jump_list_add(&to_body,emit_jump(c,OP_JUMP_IF_FALSE,value));
            }else{
                emit_op(c,OP_EQUAL,value);
                next_case = emit_jump(c,OP_JUMP_IF_FALSE,value);
            }
        }

        jump_list_patch(c,&to_body,ast_case);
        compile_statement(c,ast_case->if_body);
        jump_list_add(&ends,emit_jump(c,OP_JUMP,ast_case));
        patch_jump(c,next_case,ast_case);
    }

    if(node->else_body != NULL){
        compile_statement(c,node->else_body);
    }else{
        emit_op_u8(c,OP_GET_LOCAL,subject,node);
        emit_op_u8(c,OP_MATCH_FAIL,has_enum ? 1 : 0,node);
    }

    jump_list_patch(c,&ends,node);
    end_scope(c,node);
}

static void loop_begin(Compiler *c, Loop *loop, int break_depth, int continue_depth, int continue_target){
    *loop = (Loop){.enclosing = c->current->loop,.break_depth = break_depth,
        .continue_depth = continue_depth,.continue_target = continue_target};
    c->current->loop = loop;
}

static void loop_end(Compiler *c, Loop *loop, ast_t *node){
    jump_list_patch(c,&loop->breaks,node);
    c->current->loop = loop->enclosing;
}

static void compile_break(Compiler *c, ast_t *node){
    Loop *loop = c->current->loop;
    emit_leave(c,loop->break_depth,node);
    jump_list_add(&loop->breaks,emit_jump(c,OP_JUMP,node));
}

static void compile_continue(Compiler *c, ast_t *node){
    Loop *loop = c->current->loop;
    emit_leave(c,loop->continue_depth,node);
    if(loop->continue_target >= 0)
        emit_loop(c,loop->continue_target,node);
    else
        jump_list_add(&loop->continues,emit_jump(c,OP_JUMP,node));
}

static void compile_while(Compiler *c, ast_t *node){
    int start = current_chunk(c)->count;
    JumpList false_jumps = {0};
    compile_condition(c,node->while_expr,&false_jumps);

    Loop loop;
    int depth = c->current->scope_depth;
    loop_begin(c,&loop,depth,depth,start);
    compile_statement(c,node->while_body);
    emit_loop(c,start,node);

    jump_list_patch(c,&false_jumps,node);
    loop_end(c,&loop,node);
}

// for init ; condition ; actions { } : une seule portée pour toute la boucle.
static void compile_for(Compiler *c, ast_t *node){
    begin_scope(c);
    if(node->for_init != NULL)
        compile_statement(c,node->for_init);

    int start = current_chunk(c)->count;
    JumpList false_jumps = {0};
    if(node->for_condition != NULL)
        compile_condition(c,node->for_condition,&false_jumps);

    Loop loop;
    int depth = c->current->scope_depth;
    loop_begin(c,&loop,depth,depth,-1);
    compile_statement(c,node->for_body);

    jump_list_patch(c,&loop.continues,node);
    compile_statements(c,node->for_solve);
    emit_loop(c,start,node);

    jump_list_patch(c,&false_jumps,node);
    loop_end(c,&loop,node);
    end_scope(c,node);
}

static void declare_for_in_variable(Compiler *c, ast_t *variable){
    int slot = add_local(c,string_from(c->vm,variable->variable_definition_name),variable);
    c->current->locals[slot].type = make_type(c,variable->variable_definition_static_type_name);
}

/**
 * @brief le parcours occupe trois cases cachées (OP_ITER_PREP). Chaque tour a sa
 *        portée : une lfunc du corps garde les variables de ce tour-là.
 */
static void compile_for_in(Compiler *c, ast_t *node){
    begin_scope(c);
    int depth = c->current->scope_depth;

    compile_expr(c,node->for_condition);
    emit_op(c,OP_ITER_PREP,node);
    for(int i = 0; i < 3; i++)
        add_local(c,NULL,node);

    ast_t *first = node->for_init;
    ast_t *second = first->left;

    int start = current_chunk(c)->count;
    emit_op_u8(c,OP_ITER_NEXT,second != NULL ? 2 : 1,node);
    emit_u16(c,0xFFFF,node);
    int exit = current_chunk(c)->count - 2;

    begin_scope(c);
    declare_for_in_variable(c,first);
    Local *x = &c->current->locals[c->current->local_count - 1];
    if(second != NULL){
        declare_for_in_variable(c,second);
        Local *y = x + 1;
        if(x->type.kind != TYPE_ANY){
            emit_op_u8(c,OP_GET_LOCAL,c->current->local_count - 2,first);
            emit_check(c,CHECK_VARIABLE,x->type,x->name,first);
            emit_op(c,OP_POP,first);
        }
        emit_check(c,CHECK_VARIABLE,y->type,y->name,second);
    }else{
        emit_check(c,CHECK_VARIABLE,x->type,x->name,first);
    }

    Loop loop;
    loop_begin(c,&loop,depth,depth + 1,-1);
    compile_statement(c,node->for_body);
    jump_list_patch(c,&loop.continues,node);
    end_scope(c,node);
    emit_loop(c,start,node);

    patch_jump(c,exit,node);
    loop_end(c,&loop,node);
    end_scope(c,node);
}


//                          ===================================== $MODULES ==============================================


static void compile_include(Compiler *c, ast_t *node){
    int path = name_constant(c,node->variable_name,node);

    if(node->is_dll){
        if(node->is_import_all)
            visitor_error(node,"une bibliothèque native s'importe par noms : `import [a, b]`");

        for(int i = 0; i < node->list_compound->Size; i++){
            ast_t *name = List_Get(node->list_compound,i);
            emit_op_u16(c,OP_IMPORT_NATIVE,path,node);
            emit_u16(c,name_constant(c,name->variable_name,name),name);
            if(is_global_scope(c))
                add_file_name(c,name->variable_name);
            define_top(c,string_from(c->vm,name->variable_name),DEF_VARIABLE,false,name);
        }
        return;
    }

    emit_op_u16(c,OP_IMPORT,path,node);

    if(node->is_import_all){
        c->has_import_all = true;
        emit_op(c,OP_IMPORT_ALL,node);
        return;
    }

    // Dans un bloc, le module attend dans une case cachée et chaque nom devient une locale.
    if(!is_global_scope(c)){
        int module = add_local(c,NULL,node);
        for(int i = 0; i < node->list_compound->Size; i++){
            ast_t *name = List_Get(node->list_compound,i);
            emit_op_u8(c,OP_GET_LOCAL,module,name);
            emit_op_u16(c,OP_IMPORT_VALUE,name_constant(c,name->variable_name,name),name);
            define_top(c,string_from(c->vm,name->variable_name),DEF_VARIABLE,false,name);
        }
        return;
    }

    // OP_IMPORT_NAME définit lui-même la globale : elle garde son type et sa constance.
    for(int i = 0; i < node->list_compound->Size; i++){
        ast_t *name = List_Get(node->list_compound,i);
        add_file_name(c,name->variable_name);
        global_index(c,string_from(c->vm,name->variable_name),name);
        emit_op_u16(c,OP_IMPORT_NAME,name_constant(c,name->variable_name,name),name);
    }
    emit_op(c,OP_POP,node);
}

static void compile_export(Compiler *c, ast_t *node){
    for(int i = 0; i < node->list_compound->Size; i++){
        ast_t *name = List_Get(node->list_compound,i);
        emit_op_u16(c,OP_EXPORT,name_constant(c,name->variable_name,name),name);
    }
}


//                          ===================================== $SWITCH ==============================================


static void compile_expr(Compiler *c, ast_t *node){
    if(ceflo_stack_exceeded())
        visitor_error(node,TOO_DEEP);

    switch(node->type){
        case AST_NONE: emit_op(c,OP_NONE,node); break;
        case AST_BOOL: emit_op(c,node->bool_value ? OP_TRUE : OP_FALSE,node); break;
        case AST_CHAR:
        case AST_NUMBER:
        case AST_STRING: emit_constant(c,constant_of(c,node),node); break;
        case AST_LIST: compile_list(c,node); break;
        case AST_MAP: compile_map(c,node); break;
        case AST_LFUNC: compile_function(c,node,FUNC_FUNCTION,NULL); break;

        case AST_VARIABLE:{
            Name name = resolve_name(c,node->variable_name,node);
            emit_get_name(c,&name,node);
            break;
        }
        case AST_OBJ_ACCESS:{
            Name unused = {.kind = NAME_GLOBAL};
            int slot = self_field_slot(c,node,&unused);
            if(slot >= 0){
                emit_op_u8(c,OP_GET_SELF_FIELD,slot,node);
                break;
            }
            compile_expr(c,node->left);
            emit_op_u16(c,OP_GET_FIELD,name_constant(c,node->variable_name,node),node);
            break;
        }
        case AST_LIST_ACCESS:
            compile_expr(c,node->left);
            compile_expr(c,node->list_ptr);
            emit_op(c,OP_GET_INDEX,node);
            break;
        case AST_FUNCTION_CALL: compile_call(c,node); break;
        case AST_STRUCT: compile_new(c,node); break;
        case AST_TYPE_CALL: compile_type_call(c,node); break;
        case AST_BINOP: compile_binop(c,node); break;
        case AST_UNOP: compile_unop(c,node); break;
        case AST_TERNARY: compile_ternary(c,node); break;

        default: visitor_error(node,"noeud inconnu");
    }
}

static void compile_statement(Compiler *c, ast_t *node){
    if(ceflo_stack_exceeded())
        visitor_error(node,TOO_DEEP);

    switch(node->type){
        case AST_NOOP:
        case AST_CASE: break;       // lu par compile_match
        case AST_COMPOUND: compile_compound(c,node); break;
        case AST_VARIABLE_DEFINITION: compile_variable_definition(c,node); break;
        case AST_FUNCTION_DEFINITION: compile_function_definition(c,node); break;
        case AST_STRUCT_DEFINITION: compile_struct_definition(c,node); break;
        case AST_ENUM_DEFINITION: compile_enum_definition(c,node); break;
        case AST_ASSIGNEMENT: compile_assignement(c,node); break;
        case AST_RETURN: compile_return(c,node); break;
        case AST_BREAK: compile_break(c,node); break;
        case AST_CONTINUE: compile_continue(c,node); break;
        case AST_IF: compile_if(c,node); break;
        case AST_MATCH: compile_match(c,node); break;
        case AST_WHILE: compile_while(c,node); break;
        case AST_FOR: compile_for(c,node); break;
        case AST_FOR_IN: compile_for_in(c,node); break;
        case AST_INCLUDE: compile_include(c,node); break;
        case AST_EXPORT: compile_export(c,node); break;

        // Une expression seule : sa valeur ne sert à rien.
        default:
            compile_expr(c,node);
            emit_op(c,OP_POP,node);
            break;
    }
}

static void compile_statements(Compiler *c, ast_t *node){
    for(int i = 0; i < node->list_compound->Size; i++)
        compile_statement(c,List_Get(node->list_compound,i));
}


//                          ===================================== $PROGRAMME ==============================================


ObjFunction *compile_module(VM *vm, ast_t *program, ObjModule *module){
    Compiler c = {.vm = vm,.module = module};
    table_init(&c.file_names);
    scan_file_names(&c,program);

    FuncState fs;
    func_state_begin(&c,&fs,FUNC_SCRIPT,program);
    compile_statements(&c,program);
    emit_op(&c,OP_NONE,program);
    emit_op(&c,OP_RETURN,program);
    ObjFunction *function = func_state_end(&c);

    table_free(&c.file_names);
    free(c.global_kinds);
    return function;
}
