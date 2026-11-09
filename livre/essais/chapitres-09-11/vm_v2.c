/**
 * @file vm.c
 * @brief la machine virtuelle : exécute le bytecode.
 *
 * [NOTE]
 *      vm_run est la boucle d'exécution : elle lit une instruction, fait ce
 *      qu'elle dit, et passe à la suivante. Chaque `case` est court ; ce qui
 *      est rare ou lent (une erreur, une méthode de list...) est dans une
 *      fonction à part.
 *
 *      ip, slots et les constantes de la fonction en cours sont gardés dans
 *      des variables locales : c'est ce qui rend la boucle rapide. Avant tout
 *      ce qui peut afficher une erreur ou rappeler du CeFlo, SYNC() range ip
 *      dans le CallFrame, pour que vm_current_node trouve le bon noeud.
 *
 *      vm_run est réentrante : une native qui appelle vm_call lance une
 *      deuxième boucle, qui s'arrête quand la fonction appelée retourne.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>

#include "vm.h"
#include "memory.h"
#include "natives.h"
#include "error.h"

// Défini dans module.c : les instructions OP_IMPORT*, OP_EXPORT.
Value module_import(VM *vm, ObjString *path);
void module_import_name(VM *vm, ObjModule *into, ObjModule *from, ObjString *name);
Value module_import_value(VM *vm, ObjModule *from, ObjString *name);
void module_import_all(VM *vm, ObjModule *into, ObjModule *from);
void module_export(VM *vm, ObjModule *module, ObjString *name);

#define TOO_DEEP "trop d'appels imbriqués : la récursion ne s'arrête peut-être jamais"


//                          ===================================== $ERREURS ==============================================


static ast_t *frame_node(CallFrame *frame){
    Chunk *chunk = &frame->closure->function->chunk;
    int offset = (int)(frame->ip - chunk->code) - 1;
    return chunk->nodes[offset < 0 ? 0 : offset];
}

ast_t *vm_current_node(VM *vm){
    if(vm->frame_count == 0)
        return NULL;
    return frame_node(&vm->frames[vm->frame_count - 1]);
}

void vm_error(VM *vm, const char *format, ...){
    va_list args;
    va_start(args,format);
    error_node_v(vm_current_node(vm),format,args);
}

static _Noreturn __attribute__((format(printf,2,3))) void error_at(ast_t *node, const char *format, ...){
    va_list args;
    va_start(args,format);
    error_node_v(node,format,args);
}

// Un paramètre ou un retour faux se signale là où la fonction a été appelée.
static ast_t *caller_node(VM *vm){
    if(vm->frame_count >= 2)
        return frame_node(&vm->frames[vm->frame_count - 2]);
    return vm->frames[vm->frame_count - 1].closure->function->definition;
}


//                          ===================================== $TYPES ==============================================


static bool is_whole(Value value){
    return IS_NUMBER(value) && AS_NUMBER(value) == trunc(AS_NUMBER(value));
}

// Les types de base se testent ici ; une structure ou une énumération passe par typer_check.
static bool type_matches(VM *vm, ObjModule *module, Value value, TypeRef *type){
    switch(type->kind){
        case TYPE_ANY: return true;
        case TYPE_INT: return is_whole(value);
        case TYPE_FLOAT: return IS_NUMBER(value);
        case TYPE_BOOL: return IS_BOOL(value);
        case TYPE_CHAR: return IS_CHAR(value);
        case TYPE_STR: return IS_STRING(value);
        case TYPE_LIST: return IS_LIST(value);
        case TYPE_MAP: return IS_MAP(value);
        case TYPE_NAMED: return typer_check(vm,module,value,type);
    }
    return false;
}

static void check_type(VM *vm, ObjModule *module, TypeCheck *check, Value value){
    if(type_matches(vm,module,value,&check->type))
        return;

    const char *subject = check->subject->chars;
    const char *expected = check->type.name->chars;
    const char *actual = value_typer_name(value);

    switch(check->kind){
        case CHECK_VARIABLE:
            vm_error(vm,"`%s` est de type %s, pas %s",subject,expected,actual);
        case CHECK_ARGUMENT:
            error_at(caller_node(vm),"`%s` est de type %s, pas %s",subject,expected,actual);
        case CHECK_RETURN:
            error_at(caller_node(vm),"`%s` doit retourner une valeur de type %s, pas %s",subject,expected,actual);
    }
}


//                          ===================================== $APPELS ==============================================


static ObjUpvalue *capture_upvalue(VM *vm, Value *slot){
    ObjUpvalue *previous = NULL;
    ObjUpvalue *upvalue = vm->open_upvalues;
    while(upvalue != NULL && upvalue->location > slot){
        previous = upvalue;
        upvalue = upvalue->next;
    }
    if(upvalue != NULL && upvalue->location == slot)
        return upvalue;

    ObjUpvalue *created = upvalue_new(vm,slot);
    created->next = upvalue;
    if(previous == NULL)
        vm->open_upvalues = created;
    else
        previous->next = created;
    return created;
}

// Les variables capturées qui quittent la pile gardent leur valeur dans l'upvalue.
static void close_upvalues(VM *vm, Value *last){
    while(vm->open_upvalues != NULL && vm->open_upvalues->location >= last){
        ObjUpvalue *upvalue = vm->open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm->open_upvalues = upvalue->next;
    }
}

static void call_closure(VM *vm, ObjClosure *closure, int argc){
    ObjFunction *function = closure->function;
    if(argc != function->arity){
        vm_error(vm,"`%s` prend %d argument(s), mais en reçoit %d",
            function->name != NULL ? function->name->chars : "?",function->arity,argc);
    }
    if(vm->frame_count == FRAMES_MAX || vm->stack_top - vm->stack > STACK_MAX - 512)
        vm_error(vm,TOO_DEEP);

    CallFrame *frame = &vm->frames[vm->frame_count++];
    frame->closure = closure;
    frame->ip = function->chunk.code;
    frame->slots = vm->stack_top - argc - 1;
}

/*
    La pile contient [case0 a1..an]. Appelle `callee` : une closure ouvre un
    nouveau CallFrame (retourne true), une native s'exécute tout de suite et
    laisse son résultat à la place de case0 (retourne false).
*/
static bool call_value(VM *vm, Value callee, int argc){
    if(IS_OBJ(callee)){
        switch(OBJ_TYPE(callee)){
            case OBJ_CLOSURE:
                call_closure(vm,AS_CLOSURE(callee),argc);
                return true;
            case OBJ_NATIVE:{
                Value result = AS_NATIVE(callee)->function(vm,argc,vm->stack_top - argc);
                vm->stack_top -= argc + 1;
                vm_push(vm,result);
                return false;
            }
            case OBJ_BOUND_METHOD:{
                ObjBoundMethod *bound = AS_BOUND_METHOD(callee);
                vm->stack_top[-argc - 1] = bound->receiver;
                return call_value(vm,bound->method,argc);
            }
            default:
                break;
        }
    }
    vm_error(vm,"%s n'est pas une fonction",value_type_name(callee));
}

static Value vm_run(VM *vm, int base);

// Appelle `function` avec `first` en case 0 (self, ou la fonction elle-même).
static Value call_from_c(VM *vm, Value first, Value function, int argc, Value *args){
    if(ceflo_stack_exceeded())
        vm_error(vm,TOO_DEEP);

    Value *top = vm->stack_top;
    vm_push(vm,first);
    for(int i = 0; i < argc; i++)
        vm_push(vm,args[i]);

    int base = vm->frame_count;
    Value result = call_value(vm,function,argc) ? vm_run(vm,base) : vm_pop(vm);
    vm->stack_top = top;
    return result;
}

Value vm_call(VM *vm, Value function, int argc, Value *args){
    return call_from_c(vm,function,function,argc,args);
}


//                          ===================================== $OBJETS ==============================================


Value vm_new_instance(VM *vm, ObjStruct *structure, int argc, Value *args){
    Value init = argc < 8 ? structure->inits[argc] : NONE_VAL;
    if(IS_NONE(init) && argc > 0)
        vm_error(vm,"`%s` n'a pas de __init__ qui prend %d argument(s)",structure->name->chars,argc);

    Value instance = OBJ_VAL(instance_new(vm,structure));
    vm_push(vm,instance);   // gardé en vie pendant les appels
    if(!IS_NONE(structure->fields_init))
        call_from_c(vm,instance,structure->fields_init,0,NULL);
    if(!IS_NONE(init))
        call_from_c(vm,instance,init,argc,args);
    vm_pop(vm);
    return instance;
}

// Une structure désignée par son nom (une globale) ou par sa valeur.
static ObjStruct *find_struct(VM *vm, ObjModule *module, Value value, ObjString *name){
    if(IS_STRING(value)){
        Value index;
        ObjString *global = AS_STRING(value);
        value = UNDEFINED_VAL;
        if(table_get(&module->indexes,global,&index))
            value = module->globals.values[(int)AS_NUMBER(index)];
    }
    if(IS_ENUM(value))
        vm_error(vm,"`%s` est une énumération : ses valeurs existent déjà",name->chars);
    if(!IS_STRUCT(value))
        vm_error(vm,"la structure `%s` n'est pas définie",name->chars);
    return AS_STRUCT(value);
}

static Value new_struct(VM *vm, ObjStruct *model){
    ObjStruct *structure = struct_new(vm,model->name,model->definition,model->field_count);
    int count = model->field_count;
    if(count > 0){
        memcpy(structure->field_names,model->field_names,sizeof(ObjString*) * count);
        memcpy(structure->field_types,model->field_types,sizeof(TypeRef) * count);
        memcpy(structure->field_is_const,model->field_is_const,sizeof(bool) * count);
    }
    return OBJ_VAL(structure);
}

// Chaque exécution de la définition crée une énumération, avec ses propres éléments.
static Value new_enum(VM *vm, ObjEnum *model){
    ObjEnum *enumeration = enum_new(vm,model->name,model->definition,model->count);
    for(int i = 0; i < model->count; i++)
        enumeration->items[i] = enum_value_new(vm,enumeration,model->items[i]->name,model->items[i]->value);
    return OBJ_VAL(enumeration);
}

static void define_method(VM *vm, ObjStruct *structure, ObjString *name, Value method){
    if(name == vm->name_init)
        structure->inits[AS_CLOSURE(method)->function->arity] = method;
    else if(name == vm->name_str)
        structure->str_method = method;
    else if(name == vm->name_cast)
        structure->cast_method = method;
    else
        table_set(&structure->methods,name,method);
}

static ObjEnumValue *enum_find(ObjEnum *enumeration, ObjString *name){
    for(int i = 0; i < enumeration->count; i++){
        if(enumeration->items[i]->name == name)
            return enumeration->items[i];
    }
    return NULL;
}

// o.nom quand o n'est pas un objet dont `nom` est un attribut.
static Value get_field_slow(VM *vm, Value object, ObjString *name){
    if(IS_INSTANCE(object)){
        Value method;
        if(table_get(&AS_INSTANCE(object)->structure->methods,name,&method))
            return OBJ_VAL(bound_method_new(vm,object,method));
        vm_error(vm,"`%s` n'a pas d'attribut `%s`",value_type_name(object),name->chars);
    }

    if(IS_MAP(object)){
        Value value;
        return map_get(AS_MAP(object),OBJ_VAL(name),&value) ? value : NONE_VAL;
    }

    if(IS_ENUM(object)){
        ObjEnumValue *item = enum_find(AS_ENUM(object),name);
        if(item == NULL)
            vm_error(vm,"`%s` n'a pas de valeur `%s`",AS_ENUM(object)->name->chars,name->chars);
        return OBJ_VAL(item);
    }

    if(IS_ENUM_VALUE(object)){
        if(strcmp(name->chars,"name") == 0)
            return OBJ_VAL(AS_ENUM_VALUE(object)->name);
        if(strcmp(name->chars,"value") == 0)
            return AS_ENUM_VALUE(object)->value;
        vm_error(vm,"`%s` n'a pas d'attribut `%s`",value_type_name(object),name->chars);
    }

    vm_error(vm,"%s n'a pas d'attribut `%s`",value_type_name(object),name->chars);
}

static void set_field_slow(VM *vm, Value object, ObjString *name, Value value){
    if(IS_MAP(object)){
        map_set(vm,AS_MAP(object),OBJ_VAL(name),value);
        return;
    }
    if(IS_ENUM(object) || IS_ENUM_VALUE(object))
        vm_error(vm,"une énumération ne se modifie pas");
    if(IS_INSTANCE(object)){
        Value method;
        if(table_get(&AS_INSTANCE(object)->structure->methods,name,&method))
            vm_error(vm,"`%s` n'est pas un attribut",name->chars);
        vm_error(vm,"`%s` n'a pas d'attribut `%s`",value_type_name(object),name->chars);
    }
    vm_error(vm,"%s n'a pas d'attribut `%s`",value_type_name(object),name->chars);
}

// Un attribut typé ou constant : la closure qui initialise les attributs peut écrire une constante.
static void check_field(VM *vm, ObjModule *module, ObjInstance *instance, int index, Value value){
    ObjStruct *structure = instance->structure;
    if(structure->field_is_const[index]){
        ObjClosure *current = vm->frames[vm->frame_count - 1].closure;
        if(IS_NONE(structure->fields_init) || AS_CLOSURE(structure->fields_init) != current)
            vm_error(vm,"`%s` est une constante",structure->field_names[index]->chars);
    }
    TypeRef *type = &structure->field_types[index];
    if(!type_matches(vm,module,value,type)){
        vm_error(vm,"`%s` est de type %s, pas %s",
            structure->field_names[index]->chars,type->name->chars,value_typer_name(value));
    }
}

/*
    o.nom(args) : la pile contient [o a1..an]. Retourne true si un CallFrame
    a été ouvert. Même ordre que l'ancien interpréteur : une fonction rangée
    dans un map, un attribut, une méthode de la structure, puis les méthodes
    de list, map, str et enum.
*/
static bool invoke(VM *vm, ObjString *name, int argc, ast_t *name_node){
    Value receiver = vm->stack_top[-argc - 1];

    if(IS_INSTANCE(receiver)){
        ObjInstance *instance = AS_INSTANCE(receiver);
        int index = struct_field_index(instance->structure,name);
        if(index >= 0){
            vm->stack_top[-argc - 1] = instance->fields[index];
            return call_value(vm,instance->fields[index],argc);
        }
        Value method;
        if(!table_get(&instance->structure->methods,name,&method))
            error_at(name_node,"`%s` n'a pas de méthode `%s`",value_type_name(receiver),name->chars);
        return call_value(vm,method,argc);
    }

    if(IS_MAP(receiver)){
        Value held;
        if(map_get(AS_MAP(receiver),OBJ_VAL(name),&held) &&
           (IS_CLOSURE(held) || IS_NATIVE(held) || IS_BOUND_METHOD(held))){
            vm->stack_top[-argc - 1] = held;
            return call_value(vm,held,argc);
        }
    }

    bool found = false;
    Value result = natives_call_method(vm,receiver,name,argc,vm->stack_top - argc,&found);
    if(!found)
        error_at(name_node,"%s n'a pas de méthode `%s`",value_type_name(receiver),name->chars);
    vm->stack_top -= argc + 1;
    vm_push(vm,result);
    return false;
}


//                          ===================================== $INDEX ==============================================


static int get_index(VM *vm, Value index, int size){
    if(!IS_NUMBER(index))
        vm_error(vm,"un index doit être un nombre, pas %s",value_type_name(index));

    double position = AS_NUMBER(index);
    if(position != floor(position))
        vm_error(vm,"un index doit être un entier");
    if(position < 0 || position >= size)
        vm_error(vm,"l'index %.0f n'existe pas (taille : %d)",position,size);

    return (int)position;
}

static void check_map_key(VM *vm, Value key){
    if(!map_key_is_valid(key))
        vm_error(vm,"une clé de map doit être str, number ou un élément d'énumération, pas %s",value_type_name(key));
}

static Value get_index_slow(VM *vm, Value container, Value index){
    if(IS_LIST(container))
        return AS_LIST(container)->items[get_index(vm,index,AS_LIST(container)->count)];
    if(IS_STRING(container))
        return CHAR_VAL(AS_CSTRING(container)[get_index(vm,index,AS_STRING(container)->length)]);
    if(IS_MAP(container)){
        check_map_key(vm,index);
        Value value;
        return map_get(AS_MAP(container),index,&value) ? value : NONE_VAL;
    }
    vm_error(vm,"%s ne s'utilise pas avec `[ ]`",value_type_name(container));
}

// s[i] = 'x' : une chaîne ne change jamais, on en crée une nouvelle.
static Value set_char(VM *vm, ObjString *string, int index, Value value){
    char carac = '\0';
    if(IS_CHAR(value))
        carac = AS_CHAR(value);
    else if(IS_NUMBER(value))
        carac = isfinite(AS_NUMBER(value)) ? (char)(int)fmod(AS_NUMBER(value),256) : '\0';
    else
        vm_error(vm,"un caractère de chaîne reçoit un char, pas %s",value_type_name(value));

    char *copy = malloc((size_t)string->length + 1);
    memcpy(copy,string->chars,(size_t)string->length + 1);
    copy[index] = carac;
    ObjString *result = string_new(vm,copy,string->length);
    free(copy);
    return OBJ_VAL(result);
}

// Retourne true pour une liste ou un map (c'est fini) ; false pour une chaîne, dont la copie est sur la pile.
static bool set_index(VM *vm, Value container, Value index, Value value){
    if(IS_LIST(container)){
        AS_LIST(container)->items[get_index(vm,index,AS_LIST(container)->count)] = value;
        return true;
    }
    if(IS_MAP(container)){
        check_map_key(vm,index);
        map_set(vm,AS_MAP(container),index,value);
        return true;
    }
    if(IS_STRING(container)){
        int position = get_index(vm,index,AS_STRING(container)->length);
        vm_push(vm,set_char(vm,AS_STRING(container),position,value));
        return false;
    }
    vm_error(vm,"%s ne s'utilise pas avec `[ ]`",value_type_name(container));
}


//                          ===================================== $DIVERS ==============================================


static Value make_list(VM *vm, int count){
    ObjList *list = list_new(vm);
    for(int i = count; i > 0; i--)
        list_append(vm,list,vm->stack_top[-i]);
    vm->stack_top -= count;
    return OBJ_VAL(list);
}

static Value make_map(VM *vm, int count){
    ObjMap *map = map_new(vm);
    Value *pairs = vm->stack_top - 2 * count;
    for(int i = 0; i < count; i++){
        check_map_key(vm,pairs[2 * i]);
        map_set(vm,map,pairs[2 * i],pairs[2 * i + 1]);
    }
    vm->stack_top = pairs;
    return OBJ_VAL(map);
}

// format("a${x}b") : chaque morceau devient du texte, sur la pile pour rester en vie.
static Value format(VM *vm, int count){
    Value *parts = vm->stack_top - count;
    size_t length = 0;
    for(int i = 0; i < count; i++){
        if(!IS_STRING(parts[i]))
            parts[i] = OBJ_VAL(vm_to_string(vm,parts[i]));
        length += (size_t)AS_STRING(parts[i])->length;
    }

    if(length > (size_t)INT32_MAX)
        vm_error(vm,"chaîne trop longue");

    // Un texte court est interné par string_new ; un long s'écrit directement dans sa chaîne.
    char small[64];
    ObjString *result = length < sizeof(small) ? NULL : string_reserve(vm,(int)length);
    char *text = result != NULL ? result->chars : small;
    size_t at = 0;
    for(int i = 0; i < count; i++){
        memcpy(text + at,AS_CSTRING(parts[i]),(size_t)AS_STRING(parts[i])->length);
        at += (size_t)AS_STRING(parts[i])->length;
    }
    if(result == NULL)
        result = string_new(vm,small,(int)length);
    vm->stack_top = parts;
    return OBJ_VAL(result);
}

static Value negate(VM *vm, Value value){
    if(IS_BOOL(value))
        return NUMBER_VAL(-(double)AS_BOOL(value));
    if(IS_CHAR(value))
        return NUMBER_VAL(-(double)AS_CHAR(value));
    vm_error(vm,"l'opérateur `-` ne s'applique pas à %s",value_type_name(value));
}

// OP_GET_GLOBAL d'une globale pas encore définie : un builtin du même nom, sinon une erreur.
static Value undefined_global(VM *vm, ObjModule *module, int index){
    Value value;
    if(table_get(&vm->builtins,module->names[index],&value))
        return value;
    vm_error(vm,"`%s` n'est pas définie",module->names[index]->chars);
}

static void set_global(VM *vm, ObjModule *module, int index, Value value){
    ObjString *name = module->names[index];
    if(IS_UNDEFINED(module->globals.values[index]))
        vm_error(vm,"`%s` n'est pas définie",name->chars);
    if(module->is_const[index])
        vm_error(vm,"`%s` est une constante",name->chars);
    TypeRef *type = &module->types[index];
    if(!type_matches(vm,module,value,type))
        vm_error(vm,"`%s` est de type %s, pas %s",name->chars,type->name->chars,value_typer_name(value));
    module->globals.values[index] = value;
}


//                          ===================================== $FOR IN ==============================================


// Les clés d'un map sont relevées avant le premier tour.
static Value iter_prep(VM *vm, Value container){
    if(IS_MAP(container)){
        ObjMap *map = AS_MAP(container);
        ObjList *keys = list_new(vm);
        for(int i = 0; i < map->size; i++){
            if(!IS_UNDEFINED(map->entries[i].key))
                list_append(vm,keys,map->entries[i].key);
        }
        return OBJ_VAL(keys);
    }
    if(IS_LIST(container) || IS_STRING(container))
        return NONE_VAL;
    vm_error(vm,"%s ne se parcourt pas avec `for ... in`",value_type_name(container));
}

/*
    Un tour de for ... in : la pile contient [c clés i]. Pose la ou les
    variables du tour et retourne true, ou false si le parcours est fini.
    La taille d'une liste est relue à chaque tour : le corps peut la modifier.
*/
static bool iter_next(VM *vm, int count){
    Value *state = vm->stack_top - 3;
    Value container = state[0];
    int i = (int)AS_NUMBER(state[2]);
    Value first, second;

    if(IS_LIST(container)){
        if(i >= AS_LIST(container)->count)
            return false;
        first = NUMBER_VAL(i);
        second = AS_LIST(container)->items[i];
        i += 1;
    }else if(IS_STRING(container)){
        if(i >= AS_STRING(container)->length)
            return false;
        first = NUMBER_VAL(i);
        second = CHAR_VAL(AS_CSTRING(container)[i]);
        i += 1;
    }else{
        // Une clé enlevée pendant le parcours est sautée.
        ObjList *keys = AS_LIST(state[1]);
        for(;;){
            if(i >= keys->count)
                return false;
            first = keys->items[i++];
            if(map_get(AS_MAP(container),first,&second))
                break;
        }
        // Avec une seule variable, un map donne ses clés.
        if(count == 1)
            second = first;
    }

    state[2] = NUMBER_VAL(i);
    if(count == 2)
        vm_push(vm,first);
    vm_push(vm,second);
    return true;
}


//                          ===================================== $BOUCLE ==============================================


#ifdef CEFLO_GC_STRESS
    #define SAFEPOINT() gc_safepoint(vm)
#else
    #define SAFEPOINT() do{ if(vm->bytes_allocated > vm->next_gc) gc_safepoint(vm); }while(0)
#endif

#if 0
    #define USE_COMPUTED_GOTO 1
#endif

static Value vm_run(VM *vm, int base){
    CallFrame *frame;
    uint8_t *ip;
    Value *slots;
    Value *constants;
    ObjModule *module;

    #define LOAD_FRAME() do{                                            \
        frame = &vm->frames[vm->frame_count - 1];                       \
        ip = frame->ip;                                                 \
        slots = frame->slots;                                           \
        constants = frame->closure->function->chunk.constants.values;   \
        module = frame->closure->function->module;                      \
    }while(0)

    #define SYNC() (frame->ip = ip)
    #define READ_BYTE() (*ip++)
    #define READ_SHORT() (ip += 2,(uint16_t)((ip[-2] << 8) | ip[-1]))
    #define READ_CONSTANT() (constants[READ_SHORT()])
    #define READ_STRING() AS_STRING(READ_CONSTANT())
    #define PUSH(v) vm_push(vm,(v))
    #define POP() vm_pop(vm)
    #define PEEK(n) (vm->stack_top[-1 - (n)])

    // Un appel ou un retour : on reprend la fonction qui est maintenant en haut.
    #define CALLED(opened) do{ if(opened){ LOAD_FRAME(); SAFEPOINT(); } }while(0)

    // Deux nombres : le cas rapide. Le reste suit vm_binary.
    #define BINARY(op,result) do{                                       \
        Value b = PEEK(0), a = PEEK(1);                                 \
        if(IS_NUMBER(a) && IS_NUMBER(b)){                               \
            vm->stack_top -= 1;                                         \
            vm->stack_top[-1] = result;                                 \
        }else{                                                          \
            SYNC();                                                     \
            Value r = vm_binary(vm,op,a,b);                             \
            vm->stack_top -= 1;                                         \
            vm->stack_top[-1] = r;                                      \
        }                                                               \
    }while(0)
    #define NUM_A AS_NUMBER(a)
    #define NUM_B AS_NUMBER(b)

    #ifdef USE_COMPUTED_GOTO
        #define LABEL(op) [op] = &&L_##op
        static void *labels[OP_COUNT] = {
            LABEL(OP_CONSTANT), LABEL(OP_NONE), LABEL(OP_TRUE), LABEL(OP_FALSE),
            LABEL(OP_POP), LABEL(OP_DUP), LABEL(OP_DUP2),
            LABEL(OP_GET_LOCAL), LABEL(OP_SET_LOCAL), LABEL(OP_GET_UPVALUE), LABEL(OP_SET_UPVALUE),
            LABEL(OP_GET_GLOBAL), LABEL(OP_SET_GLOBAL), LABEL(OP_DEFINE_GLOBAL), LABEL(OP_GET_BUILTIN),
            LABEL(OP_CLOSE_UPVALUE), LABEL(OP_CHECK_TYPE),
            LABEL(OP_ADD), LABEL(OP_SUBTRACT), LABEL(OP_MULTIPLY), LABEL(OP_DIVIDE), LABEL(OP_MODULO),
            LABEL(OP_EQUAL), LABEL(OP_NOT_EQUAL), LABEL(OP_LESS), LABEL(OP_LESS_EQUAL),
            LABEL(OP_GREATER), LABEL(OP_GREATER_EQUAL), LABEL(OP_NEGATE), LABEL(OP_NOT), LABEL(OP_TO_BOOL),
            LABEL(OP_JUMP), LABEL(OP_JUMP_IF_FALSE), LABEL(OP_LOOP),
            LABEL(OP_CALL), LABEL(OP_INVOKE), LABEL(OP_CLOSURE), LABEL(OP_RETURN),
            LABEL(OP_LIST), LABEL(OP_MAP), LABEL(OP_GET_INDEX), LABEL(OP_SET_INDEX),
            LABEL(OP_GET_FIELD), LABEL(OP_SET_FIELD), LABEL(OP_FORMAT),
            LABEL(OP_STRUCT), LABEL(OP_METHOD), LABEL(OP_FIELDS_INIT), LABEL(OP_NEW),
            LABEL(OP_TYPE_CALL), LABEL(OP_ENUM),
            LABEL(OP_ITER_PREP), LABEL(OP_ITER_NEXT), LABEL(OP_MATCH_FAIL),
            LABEL(OP_IMPORT), LABEL(OP_IMPORT_NAME), LABEL(OP_IMPORT_VALUE), LABEL(OP_IMPORT_ALL), LABEL(OP_IMPORT_NATIVE),
            LABEL(OP_EXPORT),
            LABEL(OP_SET_LOCAL_POP), LABEL(OP_ADD_LOCAL), LABEL(OP_GET_SELF_FIELD), LABEL(OP_SET_SELF_FIELD),
        };
        #define CASE(op) L_##op
        #define NEXT() goto *labels[READ_BYTE()]
    #else
        #define CASE(op) case op
        #define NEXT() goto next
    #endif

    LOAD_FRAME();

#ifdef USE_COMPUTED_GOTO
    NEXT();
    {
#else
    next:
    switch(READ_BYTE()){
#endif

    // ======== valeurs ========

    CASE(OP_CONSTANT): PUSH(READ_CONSTANT()); NEXT();
    CASE(OP_NONE): PUSH(NONE_VAL); NEXT();
    CASE(OP_TRUE): PUSH(BOOL_VAL(true)); NEXT();
    CASE(OP_FALSE): PUSH(BOOL_VAL(false)); NEXT();
    CASE(OP_POP): vm->stack_top--; NEXT();
    // ======== variables ========

    CASE(OP_GET_LOCAL): PUSH(slots[READ_BYTE()]); NEXT();
    CASE(OP_SET_LOCAL): slots[READ_BYTE()] = PEEK(0); NEXT();
    CASE(OP_GET_GLOBAL):{
        int index = READ_SHORT();
        Value value = module->globals.values[index];
        if(IS_UNDEFINED(value)){
            SYNC();
            value = undefined_global(vm,module,index);
        }
        PUSH(value);
        NEXT();
    }
    CASE(OP_SET_GLOBAL):{
        int index = READ_SHORT();
        SYNC();
        set_global(vm,module,index,PEEK(0));
        NEXT();
    }
    CASE(OP_DEFINE_GLOBAL):{
        int index = READ_SHORT();
        module->globals.values[index] = POP();
        NEXT();
    }
    CASE(OP_GET_BUILTIN):{
        ObjString *name = READ_STRING();
        Value value;
        if(!table_get(&vm->builtins,name,&value)){
            SYNC();
            vm_error(vm,"`%s` n'est pas définie",name->chars);
        }
        PUSH(value);
        NEXT();
    }
    // ======== vérifications de type ========

    // ======== opérateurs ========

    CASE(OP_ADD): BINARY(OP_ADD,NUMBER_VAL(NUM_A + NUM_B)); NEXT();
    CASE(OP_SUBTRACT): BINARY(OP_SUBTRACT,NUMBER_VAL(NUM_A - NUM_B)); NEXT();
    CASE(OP_MULTIPLY): BINARY(OP_MULTIPLY,NUMBER_VAL(NUM_A * NUM_B)); NEXT();
    CASE(OP_LESS): BINARY(OP_LESS,BOOL_VAL(NUM_A < NUM_B)); NEXT();
    CASE(OP_LESS_EQUAL): BINARY(OP_LESS_EQUAL,BOOL_VAL(NUM_A <= NUM_B)); NEXT();
    CASE(OP_GREATER): BINARY(OP_GREATER,BOOL_VAL(NUM_A > NUM_B)); NEXT();
    CASE(OP_GREATER_EQUAL): BINARY(OP_GREATER_EQUAL,BOOL_VAL(NUM_A >= NUM_B)); NEXT();

    // Une division par zéro passe par vm_binary, qui affiche l'erreur.
    CASE(OP_DIVIDE):
        if(IS_NUMBER(PEEK(0)) && AS_NUMBER(PEEK(0)) == 0)
            goto divide_slow;
        BINARY(OP_DIVIDE,NUMBER_VAL(NUM_A / NUM_B));
        NEXT();
    divide_slow:{
        SYNC();
        Value r = vm_binary(vm,OP_DIVIDE,PEEK(1),PEEK(0));
        vm->stack_top -= 1;
        vm->stack_top[-1] = r;
        NEXT();
    }
    CASE(OP_MODULO):
        if(IS_NUMBER(PEEK(0)) && AS_NUMBER(PEEK(0)) == 0)
            goto modulo_slow;
        BINARY(OP_MODULO,NUMBER_VAL(fmod(NUM_A,NUM_B)));
        NEXT();
    modulo_slow:{
        SYNC();
        Value r = vm_binary(vm,OP_MODULO,PEEK(1),PEEK(0));
        vm->stack_top -= 1;
        vm->stack_top[-1] = r;
        NEXT();
    }

    CASE(OP_EQUAL):{
        Value b = POP(), a = PEEK(0);
        bool equal = IS_NUMBER(a) && IS_NUMBER(b) ? NUM_A == NUM_B : value_equals(a,b);
        vm->stack_top[-1] = BOOL_VAL(equal);
        NEXT();
    }
    CASE(OP_NOT_EQUAL):{
        Value b = POP(), a = PEEK(0);
        bool equal = IS_NUMBER(a) && IS_NUMBER(b) ? NUM_A == NUM_B : value_equals(a,b);
        vm->stack_top[-1] = BOOL_VAL(!equal);
        NEXT();
    }
    CASE(OP_NEGATE):{
        Value value = PEEK(0);
        if(IS_NUMBER(value)){
            vm->stack_top[-1] = NUMBER_VAL(-AS_NUMBER(value));
        }else{
            SYNC();
            vm->stack_top[-1] = negate(vm,value);
        }
        NEXT();
    }
    CASE(OP_NOT): vm->stack_top[-1] = BOOL_VAL(!value_is_true(PEEK(0))); NEXT();
    CASE(OP_TO_BOOL): vm->stack_top[-1] = BOOL_VAL(value_is_true(PEEK(0))); NEXT();

    // ======== sauts ========

    CASE(OP_JUMP):{
        int distance = READ_SHORT();
        ip += distance;
        NEXT();
    }
    CASE(OP_JUMP_IF_FALSE):{
        int distance = READ_SHORT();
        Value condition = POP();
        bool truth = IS_BOOL(condition) ? AS_BOOL(condition) : value_is_true(condition);
        if(!truth)
            ip += distance;
        NEXT();
    }
    CASE(OP_LOOP):{
        int distance = READ_SHORT();
        ip -= distance;
        SYNC();
        SAFEPOINT();
        NEXT();
    }

    // ======== fonctions ========

    CASE(OP_CALL):{
        int argc = READ_BYTE();
        Value callee = PEEK(argc);
        SYNC();
        // Une closure est le cas fréquent : sans passer par call_value.
        if(IS_CLOSURE(callee)){
            call_closure(vm,AS_CLOSURE(callee),argc);
            CALLED(true);
        }else{
            CALLED(call_value(vm,callee,argc));
        }
        NEXT();
    }
    CASE(OP_CLOSURE):{
        ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
        ObjClosure *closure = closure_new(vm,function);
        PUSH(OBJ_VAL(closure));
        for(int i = 0; i < closure->upvalue_count; i++){
            int is_local = READ_BYTE();
            int index = READ_BYTE();
            closure->upvalues[i] = is_local ? capture_upvalue(vm,slots + index) : frame->closure->upvalues[index];
        }
        NEXT();
    }
    CASE(OP_RETURN):{
        Value result = POP();
        close_upvalues(vm,slots);
        vm->frame_count--;
        vm->stack_top = slots;
        if(vm->frame_count == base)
            return result;
        PUSH(result);
        LOAD_FRAME();
        SAFEPOINT();
        NEXT();
    }

    // ======== list, map, accès ========

    // ======== structures et énumérations ========

    // ======== for ... in et match ========

    // ======== modules ========

    // ======== instructions rapides ========

    CASE(OP_SET_LOCAL_POP): slots[READ_BYTE()] = POP(); NEXT();
    CASE(OP_ADD_LOCAL):{
        Value *local = &slots[READ_BYTE()];
        Value step = READ_CONSTANT();
        if(IS_NUMBER(*local) && IS_NUMBER(step)){
            *local = NUMBER_VAL(AS_NUMBER(*local) + AS_NUMBER(step));
        }else{
            SYNC();
            *local = vm_binary(vm,OP_ADD,*local,step);
        }
        NEXT();
    }
#ifdef USE_COMPUTED_GOTO
    }
#else
    default:
        SYNC();
        vm_error(vm,"instruction inconnue %d",ip[-1]);
    }
#endif

    #undef LOAD_FRAME
    #undef SYNC
    #undef READ_BYTE
    #undef READ_SHORT
    #undef READ_CONSTANT
    #undef READ_STRING
    #undef PUSH
    #undef POP
    #undef PEEK
    #undef CALLED
    #undef BINARY
    #undef CASE
    #undef NEXT
}


//                          ===================================== $VM ==============================================


void vm_init(VM *vm, int argc, char **argv){
    // Réservés une fois : le système ne donne les pages qu'à l'usage.
    vm->frames = ceflo_alloc(sizeof(CallFrame) * FRAMES_MAX);
    vm->frame_count = 0;
    vm->stack = ceflo_alloc(sizeof(Value) * STACK_MAX);
    vm->stack_top = vm->stack;
    vm->open_upvalues = NULL;

    vm->objects = NULL;
    vm->bytes_allocated = 0;
    vm->next_gc = 1024 * 1024;
    vm->gray_stack = NULL;
    vm->gray_count = 0;
    vm->gray_capacity = 0;
    vm->root_count = 0;
    vm->name_init = vm->name_str = vm->name_cast = vm->name_main = NULL;
    vm->disassemble = false;

    table_init(&vm->strings);
    table_init(&vm->builtins);
    table_init(&vm->modules);
    table_init(&vm->native_modules);

    vm->name_init = string_from(vm,"__init__");
    vm->name_str = string_from(vm,"__str__");
    vm->name_cast = string_from(vm,"__cast__");
    vm->name_main = string_from(vm,"main");

    natives_define_core(vm,argc,argv);
}

void vm_free(VM *vm){
    // natives_free après les objets : il ferme les bibliothèques et libère les noms des natives.
    gc_free_objects(vm);
    natives_free(vm);
    table_free(&vm->builtins);
    table_free(&vm->modules);
    table_free(&vm->native_modules);
    table_free(&vm->strings);
    free(vm->stack);
    free(vm->frames);
    vm->stack = vm->stack_top = NULL;
    vm->frames = NULL;
}

void vm_protect(VM *vm, Value *value){
    if(vm->root_count == (int)(sizeof(vm->roots) / sizeof(vm->roots[0])))
        vm_error(vm,TOO_DEEP);
    vm->roots[vm->root_count++] = value;
}

void vm_unprotect(VM *vm, int count){
    vm->root_count -= count;
}

void vm_define_builtin(VM *vm, const char *name, NativeFn function){
    ObjString *key = string_from(vm,name);
    table_set(&vm->builtins,key,OBJ_VAL(native_new(vm,name,function)));
}

void vm_define_builtin_value(VM *vm, const char *name, Value value){
    table_set(&vm->builtins,string_from(vm,name),value);
}
