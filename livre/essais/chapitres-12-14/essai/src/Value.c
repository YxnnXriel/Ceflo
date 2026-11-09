#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include "include/Value.h"
#include "include/visitor.h"
#include "include/error.h"

//                          ===================================== $CREATION ==============================================

static value_t *newValue(type_ceflo type){
    value_t *value = gc_alloc(sizeof(*value),GC_VALUE);
    value->type = type;
    return value;
}

// none, true et false ne changent jamais : une seule valeur de chaque suffit.
static value_t *Value_shared(value_t **slot, type_ceflo type, bool x){
    if(*slot == NULL){
        *slot = newValue(type);
        AS_BOOL(*slot) = x;
        gc_root(*slot);
    }
    return *slot;
}

value_t *makeNone(void){
    static value_t *none;
    return Value_shared(&none,NONE,false);
}

value_t *makeBool(bool x){
    static value_t *booleans[2];
    return Value_shared(&booleans[x],BOOL,x);
}

value_t *makeChar(char x){
    value_t *value = newValue(CHAR);
    AS_CHAR(value) = x;
    return value;
}

value_t *makeNumber(double x){
    value_t *value = newValue(NUMBER);
    AS_NUMBER(value) = x;
    return value;
}

value_t *makeStringLenght(const char *str, int lenght){
    value_t *value = newValue(STRING);
    AS_STRING(value) = new_String_Lenght(str,lenght);
    gc_add_bytes(sizeof(string_t) + (size_t)lenght);
    return value;
}

value_t *makeString(const char *str){
    return makeStringLenght(str,(int)strlen(str));
}

value_t *makeList(void){
    value_t *value = newValue(LIST);
    AS_LIST(value) = newList();
    gc_add_bytes(sizeof(list_t));
    return value;
}

value_t *makeMap(void){
    value_t *value = newValue(MAP);
    AS_MAP(value) = newMap();
    gc_add_bytes(sizeof(map_t));
    return value;
}

value_t *makeObject(value_t *structure, struct SCOPE_STRUCT *self){
    value_t *value = newValue(OBJECT);
    value->as.ObjectValue.structure = structure;
    value->as.ObjectValue.self = self;
    return value;
}

value_t *makeFunction(ast_t *definition, struct SCOPE_STRUCT *closure){
    value_t *value = newValue(FUNCTION);
    value->as.functionValue.definition = definition;
    value->as.functionValue.closure = closure;
    return value;
}

value_t *makeBuiltin(const char *name, ceflo_func_t function){
    value_t *value = newValue(BUILTIN);
    value->as.builtinValue.name = name;
    value->as.builtinValue.function = function;
    return value;
}

value_t *makeStruct(ast_t *definition, struct SCOPE_STRUCT *closure){
    value_t *value = newValue(STRUCT);
    value->as.structValue.definition = definition;
    value->as.structValue.closure = closure;
    return value;
}

value_t *makeEnum(ast_t *definition){
    value_t *value = newValue(ENUM);
    value->as.enumValue.definition = definition;
    value->as.enumValue.members = NULL;
    value_t *members = makeMap();
    value->as.enumValue.members = members;

    for(int i = 0; i < definition->struct_definition_item->Size; i++){
        ast_t *item = List_Get(definition->struct_definition_item,i);
        value_t *member = newValue(ENUM_VALUE);
        member->as.enumItem.owner = value;
        member->as.enumItem.item = item;
        Value_map_set_name(members,item->variable_name,member);
    }
    return value;
}

value_t *makeGeneric(void *x){
    value_t *value = newValue(GENERIC);
    value->as.GenericValue = x;
    return value;
}

void freeValue(value_t *value){
    if(value->type == STRING)
        String_free(AS_STRING(value));
    if(value->type == LIST)
        List_Free(AS_LIST(value));
    if(value->type == MAP)
        freeMap(AS_MAP(value));
}

void Value_list_append(value_t *list, value_t *item){
    List_Append(AS_LIST(list),item);
    gc_add_bytes(sizeof(void*));
}

// Une clé est une chaîne, un nombre ou un élément d'énumération. Un caractère compte comme une chaîne d'un seul octet.
static value_t *Value_map_key(ast_t *node, value_t *key){
    if(key->type == STRING || key->type == NUMBER || key->type == ENUM_VALUE)
        return key;
    if(key->type == CHAR)
        return makeStringLenght(&AS_CHAR(key),1);
    visitor_error(node,"une clé de map doit être str, number ou un élément d'énumération, pas %s",Value_type_name(key));
}

value_t *Value_map_get(ast_t *node, value_t *map, value_t *key){
    value_t *value = map_get(AS_MAP(map),Value_map_key(node,key));
    return value != NULL ? value : makeNone();
}

void Value_map_set(ast_t *node, value_t *map, value_t *key, value_t *value){
    map_set(AS_MAP(map),Value_map_key(node,key),value);
}

value_t *Value_map_get_name(value_t *map, const char *name){
    return map_get_text(AS_MAP(map),name,(int)strlen(name));
}

// `t.nom = 1` ne crée une chaîne que si `nom` est une nouvelle clé.
void Value_map_set_name(value_t *map, const char *name, value_t *value){
    if(!map_replace_text(AS_MAP(map),name,(int)strlen(name),value))
        map_set(AS_MAP(map),makeString(name),value);
}

value_t *Value_enum_get(value_t *enumeration, const char *name){
    return Value_map_get_name(enumeration->as.enumValue.members,name);
}

value_t *Value_enum_item_name(value_t *item){
    return makeString(item->as.enumItem.item->variable_name);
}

value_t *Value_enum_item_value(value_t *item){
    ast_t *definition = item->as.enumItem.item;
    if(definition->type == AST_NUMBER)
        return makeNumber(definition->number_value);
    return makeStringLenght(definition->string_value,definition->string_lenght);
}

static bool Value_enum_item_has(ast_t *item, value_t *value){
    if(value->type == NUMBER)
        return item->type == AST_NUMBER && item->number_value == AS_NUMBER(value);
    return item->type == AST_STRING && item->string_lenght == AS_STRING(value)->lenght &&
        memcmp(item->string_value,AS_STRING(value)->str,(size_t)item->string_lenght) == 0;
}

value_t *Value_enum_find(value_t *enumeration, value_t *value){
    map_t *members = AS_MAP(enumeration->as.enumValue.members);
    for(int i = 0; i < members->size; i++){
        value_t *member = members->entries[i].value;
        if(Value_enum_item_has(member->as.enumItem.item,value))
            return member;
    }
    return NULL;
}


//                          ===================================== $QUESTIONS ==============================================


bool Value_is_true(value_t *value){
    switch(value->type){
        case NONE: return false;
        case BOOL: return AS_BOOL(value);
        case CHAR: return AS_CHAR(value) != '\0';
        case NUMBER: return AS_NUMBER(value) != 0;
        case STRING: return AS_STRING(value)->lenght > 0;
        case LIST: return AS_LIST(value)->Size > 0;
        case MAP: return AS_MAP(value)->count > 0;
        default: return true;
    }
}

static bool Value_is_numeric(value_t *value){
    return value->type == NUMBER || value->type == BOOL || value->type == CHAR;
}

double Value_as_double(value_t *value){
    switch(value->type){
        case NUMBER: return AS_NUMBER(value);
        case BOOL: return AS_BOOL(value) ? 1 : 0;
        case CHAR: return (unsigned char)AS_CHAR(value);
        default: return 0;
    }
}

// Un caractère se compare à une chaîne comme une chaîne d'un seul octet.
static bool Value_text_view(value_t *value, const char **text, int *lenght){
    if(value->type == STRING){
        *text = AS_STRING(value)->str;
        *lenght = AS_STRING(value)->lenght;
        return true;
    }
    if(value->type == CHAR){
        *text = &AS_CHAR(value);
        *lenght = 1;
        return true;
    }
    return false;
}

int Value_compare_text(value_t *left, value_t *right, bool *ok){
    const char *a, *b;
    int size_a, size_b;

    *ok = (left->type == STRING || right->type == STRING) &&
        Value_text_view(left,&a,&size_a) && Value_text_view(right,&b,&size_b);

    if(!*ok)
        return 0;

    int order = memcmp(a,b,(size_t)(size_a < size_b ? size_a : size_b));
    return order != 0 ? order : size_a - size_b;
}

bool Value_equals(value_t *left, value_t *right){
    if(left == right)
        return true;

    if(Value_is_numeric(left) && Value_is_numeric(right))
        return Value_as_double(left) == Value_as_double(right);

    bool is_text = false;
    int order = Value_compare_text(left,right,&is_text);
    if(is_text)
        return order == 0;

    if(left->type != right->type)
        return false;

    switch(left->type){
        case NONE:
            return true;
        case LIST:{
            if(AS_LIST(left)->Size != AS_LIST(right)->Size)
                return false;
            if(ceflo_stack_exceeded())
                visitor_error(NULL,"liste trop profonde pour être comparée");
            for(int i = 0; i < AS_LIST(left)->Size; i++){
                if(!Value_equals(AS_LIST(left)->Item[i],AS_LIST(right)->Item[i]))
                    return false;
            }
            return true;
        }
        case GENERIC:
            return left->as.GenericValue == right->as.GenericValue;
        default:
            return false;
    }
}

// Un caractère est un octet : on garde seulement le reste de la division par 256.
char Value_number_to_char(double number){
    return isfinite(number) ? (char)(int)fmod(number,256) : '\0';
}

const char *Value_type_name(value_t *value){
    switch(value->type){
        case NONE: return "none";
        case BOOL: return "bool";
        case CHAR: return "char";
        case NUMBER: return "number";
        case STRING: return "str";
        case LIST: return "list";
        case MAP: return "map";
        case OBJECT: return value->as.ObjectValue.structure->as.structValue.definition->struct_definition_name;
        case FUNCTION:
        case BUILTIN: return "func";
        case STRUCT: return "struct";
        case ENUM: return "enum";
        case ENUM_VALUE: return value->as.enumItem.owner->as.enumValue.definition->struct_definition_name;
        case GENERIC: return "generic";
    }
    return "?";
}


//                          ===================================== $TEXTE ==============================================


// Les entiers s'écrivent en entier : 1000000 et non 1e+06.
static void Value_number_to_buffer(buffer_t *buffer, double number){
    char text[64];

    // Le cas le plus courant, sans passer par snprintf.
    if(number == floor(number) && fabs(number) < 1e15){
        char *end = text + sizeof(text);
        char *digit = end;
        long long whole = (long long)fabs(number);
        do{
            *--digit = (char)('0' + whole % 10);
            whole /= 10;
        }while(whole > 0);
        if(number < 0)
            *--digit = '-';
        Buffer_Append(buffer,digit,(size_t)(end - digit));
        return;
    }

    snprintf(text,sizeof(text),"%.15g",number);
    Buffer_Append_Str(buffer,text);
}

static void Value_object_to_buffer(struct VISITOR_STRUCT *visitor, buffer_t *buffer, value_t *value){
    hash_t *method = scope_get(value->as.ObjectValue.self,"__str__");

    if(method == NULL){
        Buffer_Append_Char(buffer,'<');
        Buffer_Append_Str(buffer,Value_type_name(value));
        Buffer_Append_Char(buffer,'>');
        return;
    }

    Value_to_buffer(visitor,buffer,visitor_call(visitor,NULL,method->value,NULL,0));
}

// Dans une liste, on distingue "1" de 1 : les chaînes gardent leurs guillemets.
static void Value_item_to_buffer(struct VISITOR_STRUCT *visitor, buffer_t *buffer, value_t *item){
    if(item->type == STRING){
        Buffer_Append_Char(buffer,'"');
        Value_to_buffer(visitor,buffer,item);
        Buffer_Append_Char(buffer,'"');
    }else if(item->type == CHAR){
        Buffer_Append_Char(buffer,'\'');
        Value_to_buffer(visitor,buffer,item);
        Buffer_Append_Char(buffer,'\'');
    }else{
        Value_to_buffer(visitor,buffer,item);
    }
}

static void Value_list_to_buffer(struct VISITOR_STRUCT *visitor, buffer_t *buffer, value_t *value){
    list_t *list = AS_LIST(value);

    if(list->Size == 0){
        Buffer_Append_Str(buffer,"[]");
        return;
    }

    Buffer_Append_Str(buffer,"[ ");
    for(int i = 0; i < list->Size; i++){
        Value_item_to_buffer(visitor,buffer,list->Item[i]);
        if(i != list->Size - 1)
            Buffer_Append_Str(buffer," , ");
    }
    Buffer_Append_Str(buffer," ]");
}

static void Value_map_to_buffer(struct VISITOR_STRUCT *visitor, buffer_t *buffer, value_t *value){
    map_t *map = AS_MAP(value);

    if(map->count == 0){
        Buffer_Append_Str(buffer,"{}");
        return;
    }

    bool first = true;
    Buffer_Append_Str(buffer,"{ ");
    for(int i = 0; i < map->size; i++){
        if(map->entries[i].key == NULL)
            continue;
        if(!first)
            Buffer_Append_Str(buffer," , ");
        first = false;
        Value_item_to_buffer(visitor,buffer,map->entries[i].key);
        Buffer_Append_Str(buffer," : ");
        Value_item_to_buffer(visitor,buffer,map->entries[i].value);
    }
    Buffer_Append_Str(buffer," }");
}

void Value_to_buffer(struct VISITOR_STRUCT *visitor, buffer_t *buffer, value_t *value){
    switch(value->type){
        case NONE: Buffer_Append_Str(buffer,"none"); break;
        case BOOL: Buffer_Append_Str(buffer,AS_BOOL(value) ? "true" : "false"); break;
        case CHAR: Buffer_Append_Char(buffer,AS_CHAR(value)); break;
        case NUMBER: Value_number_to_buffer(buffer,AS_NUMBER(value)); break;
        case STRING: Buffer_Append(buffer,AS_STRING(value)->str,(size_t)AS_STRING(value)->lenght); break;
        case LIST:
        case MAP:
        case OBJECT:
            if(value->gc.printing){
                Buffer_Append_Str(buffer,"...");
                break;
            }
            if(ceflo_stack_exceeded())
                visitor_error(NULL,"valeur trop profonde pour être affichée");
            value->gc.printing = true;
            if(value->type == LIST)
                Value_list_to_buffer(visitor,buffer,value);
            else if(value->type == MAP)
                Value_map_to_buffer(visitor,buffer,value);
            else
                Value_object_to_buffer(visitor,buffer,value);
            value->gc.printing = false;
            break;
        case FUNCTION:
            Buffer_Append_Str(buffer,"<func ");
            Buffer_Append_Str(buffer,value->as.functionValue.definition->function_definition_name);
            Buffer_Append_Char(buffer,'>');
            break;
        case BUILTIN:
            Buffer_Append_Str(buffer,"<func ");
            Buffer_Append_Str(buffer,value->as.builtinValue.name);
            Buffer_Append_Char(buffer,'>');
            break;
        case STRUCT:
            Buffer_Append_Str(buffer,"<struct ");
            Buffer_Append_Str(buffer,value->as.structValue.definition->struct_definition_name);
            Buffer_Append_Char(buffer,'>');
            break;
        case ENUM:
            Buffer_Append_Str(buffer,"<enum ");
            Buffer_Append_Str(buffer,value->as.enumValue.definition->struct_definition_name);
            Buffer_Append_Char(buffer,'>');
            break;
        case ENUM_VALUE:
            Buffer_Append_Str(buffer,Value_type_name(value));
            Buffer_Append_Char(buffer,'.');
            Buffer_Append_Str(buffer,value->as.enumItem.item->variable_name);
            break;
        case GENERIC:
            Buffer_Append_Str(buffer,"<generic>");
            break;
    }
}

void Value_print(struct VISITOR_STRUCT *visitor, FILE *file, value_t *value){
    buffer_t buffer;
    newBuffer(&buffer);
    Value_to_buffer(visitor,&buffer,value);
    fwrite(buffer.data,1,buffer.size,file);
    Buffer_Free(&buffer);
}

value_t *Value_to_string(struct VISITOR_STRUCT *visitor, value_t *value){
    if(value->type == STRING)
        return value;

    buffer_t buffer;
    newBuffer(&buffer);
    Value_to_buffer(visitor,&buffer,value);
    value_t *string = makeStringLenght(buffer.data,(int)buffer.size);
    Buffer_Free(&buffer);
    return string;
}


//                          ===================================== $VERIFICATIONS ==========================================


void Value_check_args(ast_t *node, const char *name, int argc, int expected){
    if(argc != expected)
        visitor_error(node,"`%s` prend %d argument(s), mais en reçoit %d",name,expected,argc);
}

void Value_check_type(ast_t *node, const char *name, value_t *value, type_ceflo expected){
    static const char *names[] = {
        "none","bool","char","number","str","list","map","object","func","func","struct","enum","enum","generic"
    };

    if(value->type != expected)
        visitor_error(node,"`%s` attend un argument de type %s, pas %s",name,names[expected],Value_type_name(value));
}

static int Value_check_index(ast_t *node, const char *name, value_t *index){
    Value_check_type(node,name,index,NUMBER);
    if(AS_NUMBER(index) != floor(AS_NUMBER(index)))
        visitor_error(node,"`%s` attend un index entier",name);
    // Un index aussi grand n'existe jamais : on évite de le convertir en int.
    return fabs(AS_NUMBER(index)) < INT_MAX ? (int)AS_NUMBER(index) : -1;
}


//                          ===================================== $LIST ===================================================


static value_t *value_List_lenght(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"lenght",argc,0);
    return makeNumber(AS_LIST(self)->Size);
}

static value_t *value_List_append(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    (void)visitor;(void)node;
    for(int i = 0; i < argc; i++)
        Value_list_append(self,args[i]);
    return makeNone();
}

static value_t *value_List_get(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"get",argc,1);
    value_t *item = List_Get(AS_LIST(self),Value_check_index(node,"get",args[0]));
    return item != NULL ? item : makeNone();
}

static value_t *value_List_remove(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"remove",argc,1);
    int index = Value_check_index(node,"remove",args[0]);
    value_t *item = List_Get(AS_LIST(self),index);
    if(item == NULL)
        visitor_error(node,"`remove` : l'index %d n'existe pas (la liste contient %d éléments)",index,AS_LIST(self)->Size);
    List_Remove(AS_LIST(self),index);
    return item;
}

static value_t *value_List_print(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    (void)args;
    Value_check_args(node,"print",argc,0);
    Value_print(visitor,stdout,self);
    printf("\n");
    return makeNone();
}

static value_t *value_List_filter(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    Value_check_args(node,"filter",argc,1);

    value_t *result = makeList();
    // Index relu à chaque tour : la fonction peut modifier la liste.
    for(int i = 0; i < AS_LIST(self)->Size; i++){
        value_t *item = AS_LIST(self)->Item[i];
        gc_temp_push(item);
        if(Value_is_true(visitor_call(visitor,node,args[0],&item,1)))
            Value_list_append(result,item);
    }
    return result;
}


//                          ===================================== $MAP ====================================================


static value_t *value_Map_lenght(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"lenght",argc,0);
    return makeNumber(AS_MAP(self)->count);
}

static value_t *value_Map_set(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"set",argc,2);
    Value_map_set(node,self,args[0],args[1]);
    return self;
}

// t.get(cle) comme t[cle], et t.get(cle, 0) donne 0 si la clé n'existe pas.
static value_t *value_Map_get(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    (void)visitor;
    if(argc != 1 && argc != 2)
        visitor_error(node,"`get` prend 1 ou 2 arguments, mais en reçoit %d",argc);

    value_t *value = map_get(AS_MAP(self),Value_map_key(node,args[0]));
    if(value != NULL)
        return value;
    return argc == 2 ? args[1] : makeNone();
}

static value_t *value_Map_has(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"has",argc,1);
    return makeBool(map_get(AS_MAP(self),Value_map_key(node,args[0])) != NULL);
}

static value_t *value_Map_remove(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"remove",argc,1);
    value_t *value = map_remove(AS_MAP(self),Value_map_key(node,args[0]));
    return value != NULL ? value : makeNone();
}

value_t *Value_map_keys(value_t *map){
    value_t *result = makeList();
    for(int i = 0; i < AS_MAP(map)->size; i++){
        if(AS_MAP(map)->entries[i].key != NULL)
            Value_list_append(result,AS_MAP(map)->entries[i].key);
    }
    return result;
}

static value_t *value_Map_keys(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"keys",argc,0);
    return Value_map_keys(self);
}

static value_t *value_Map_values(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"values",argc,0);

    value_t *result = makeList();
    for(int i = 0; i < AS_MAP(self)->size; i++){
        if(AS_MAP(self)->entries[i].key != NULL)
            Value_list_append(result,AS_MAP(self)->entries[i].value);
    }
    return result;
}


//                          ===================================== $ENUM ===================================================


static value_t *value_Enum_values(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    return value_Map_values(visitor,node,self->as.enumValue.members,args,argc);
}


//                          ===================================== $STR ====================================================


static value_t *value_Str_lenght(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"lenght",argc,0);
    return makeNumber(AS_STRING(self)->lenght);
}

// "{} a {} ans".format("Yann", 21)
static value_t *value_Str_format(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    string_t *str = AS_STRING(self);
    int used = 0;

    buffer_t buffer;
    newBuffer(&buffer);

    for(int i = 0; i < str->lenght; i++){
        if(str->str[i] == '{' && i + 1 < str->lenght && str->str[i + 1] == '}'){
            if(used >= argc){
                Buffer_Free(&buffer);
                visitor_error(node,"`format` : il y a plus de {} que d'arguments (%d)",argc);
            }
            Value_to_buffer(visitor,&buffer,args[used]);
            used += 1;
            i += 1;
        }else{
            Buffer_Append_Char(&buffer,str->str[i]);
        }
    }

    value_t *result = makeStringLenght(buffer.data,(int)buffer.size);
    Buffer_Free(&buffer);
    return result;
}

// "a,b".split(",") donne [ "a" , "b" ]
static value_t *value_Str_split(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    (void)visitor;
    Value_check_args(node,"split",argc,1);
    Value_check_type(node,"split",args[0],STRING);

    string_t *str = AS_STRING(self);
    string_t *delim = AS_STRING(args[0]);
    if(delim->lenght == 0)
        visitor_error(node,"`split` : le séparateur ne peut pas être vide");

    value_t *result = makeList();
    int start = 0;
    for(int i = 0; i + delim->lenght <= str->lenght; ){
        if(memcmp(str->str + i,delim->str,(size_t)delim->lenght) == 0){
            Value_list_append(result,makeStringLenght(str->str + start,i - start));
            i += delim->lenght;
            start = i;
        }else{
            i += 1;
        }
    }
    Value_list_append(result,makeStringLenght(str->str + start,str->lenght - start));
    return result;
}


//                          ===================================== $METHODES ===============================================


typedef value_t *(*method_func_t)(struct VISITOR_STRUCT *, ast_t *, value_t *, value_t **, int);

typedef struct{
    const char *name;
    method_func_t func;
}method_t;

static const method_t attr_value_list[] = {
    {"get",&value_List_get},
    {"remove",&value_List_remove},
    {"append",&value_List_append},
    {"lenght",&value_List_lenght},
    {"length",&value_List_lenght},
    {"print",&value_List_print},
    {"filter",&value_List_filter},
    {NULL,NULL},
};

static const method_t attr_value_map[] = {
    {"get",&value_Map_get},
    {"set",&value_Map_set},
    {"has",&value_Map_has},
    {"remove",&value_Map_remove},
    {"keys",&value_Map_keys},
    {"values",&value_Map_values},
    {"lenght",&value_Map_lenght},
    {"length",&value_Map_lenght},
    {NULL,NULL},
};

static const method_t attr_value_enum[] = {
    {"values",&value_Enum_values},
    {NULL,NULL},
};

static const method_t attr_value_str[] = {
    {"lenght",&value_Str_lenght},
    {"length",&value_Str_lenght},
    {"format",&value_Str_format},
    {"split",&value_Str_split},
    {NULL,NULL},
};

value_t *Value_call_method(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, const char *name, value_t **args, int argc){
    const method_t *methods = NULL;

    if(self->type == LIST)
        methods = attr_value_list;
    else if(self->type == MAP)
        methods = attr_value_map;
    else if(self->type == ENUM)
        methods = attr_value_enum;
    else if(self->type == STRING)
        methods = attr_value_str;
    else
        return NULL;

    for(; methods->name != NULL; methods++){
        if(strcmp(methods->name,name) == 0)
            return methods->func(visitor,node,self,args,argc);
    }

    return NULL;
}
