#ifndef __VALUE_H__
#define __VALUE_H__

    #include <stdio.h>

    #include "gc.h"
    #include "str.h"
    #include "list.h"
    #include "map.h"
    #include "ast.h"

    struct SCOPE_STRUCT;
    struct VISITOR_STRUCT;
    struct VALUE_STRUCT;

    typedef enum{
        NONE,
        BOOL,
        CHAR,
        NUMBER,
        STRING,
        LIST,
        MAP,
        OBJECT,
        FUNCTION,
        BUILTIN,
        STRUCT,
        ENUM,
        ENUM_VALUE,
        GENERIC,
    }type_ceflo;

    // Une fonction écrite en C : les builtins et les bibliothèques natives (.so)
    typedef struct VALUE_STRUCT *(*ceflo_func_t)(
        struct VISITOR_STRUCT *visitor,
        ast_t *node,
        struct VALUE_STRUCT **args,
        int argc
    );

    // Une fonction CeFlo se souvient de la portée où elle a été créée.
    typedef struct{
        ast_t *definition;
        struct SCOPE_STRUCT *closure;
    }Func_t;

    typedef struct{
        const char *name;
        ceflo_func_t function;
    }Builtin_t;

    typedef struct{
        ast_t *definition;
        struct SCOPE_STRUCT *closure;
    }Struct_t;

    // Un objet est une portée qui contient ses attributs et ses méthodes.
    typedef struct{
        struct VALUE_STRUCT *structure;
        struct SCOPE_STRUCT *self;
    }Obj_t;

    // Une énumération range ses éléments dans un map, de leur nom vers leur valeur.
    typedef struct{
        ast_t *definition;
        struct VALUE_STRUCT *members;
    }Enum_t;

    // Chaque élément n'existe qu'une fois : deux éléments se comparent par leur adresse.
    typedef struct{
        struct VALUE_STRUCT *owner;
        ast_t *item;
    }EnumValue_t;

    typedef struct VALUE_STRUCT{
        gc_object_t gc;
        type_ceflo type;
        union{
            bool booleanValue;
            char charValue;
            double numberValue;
            string_t *stringValue;
            list_t *listValue;
            map_t *mapValue;
            Obj_t ObjectValue;
            Func_t functionValue;
            Builtin_t builtinValue;
            Struct_t structValue;
            Enum_t enumValue;
            EnumValue_t enumItem;
            void *GenericValue;
        }as;
    }value_t;

    #define AS_BOOL(value) ((value)->as.booleanValue)
    #define AS_CHAR(value) ((value)->as.charValue)
    #define AS_NUMBER(value) ((value)->as.numberValue)
    #define AS_STRING(value) ((value)->as.stringValue)
    #define AS_LIST(value) ((value)->as.listValue)
    #define AS_MAP(value) ((value)->as.mapValue)

    // Créer une valeur. Elles sont toutes gérées par le garbage collector.
    value_t *makeNone(void);
    value_t *makeBool(bool x);
    value_t *makeChar(char x);
    value_t *makeNumber(double x);
    value_t *makeString(const char *str);
    value_t *makeStringLenght(const char *str, int lenght);
    value_t *makeList(void);
    value_t *makeMap(void);
    value_t *makeObject(value_t *structure, struct SCOPE_STRUCT *self);
    value_t *makeFunction(ast_t *definition, struct SCOPE_STRUCT *closure);
    value_t *makeBuiltin(const char *name, ceflo_func_t function);
    value_t *makeStruct(ast_t *definition, struct SCOPE_STRUCT *closure);
    value_t *makeEnum(ast_t *definition);
    value_t *makeGeneric(void *x);
    void freeValue(value_t *value);

    void Value_list_append(value_t *list, value_t *item);

    // Lire et écrire dans un map : t[cle] et t.nom. Une clé absente se lit comme `none`.
    value_t *Value_map_get(ast_t *node, value_t *map, value_t *key);
    void Value_map_set(ast_t *node, value_t *map, value_t *key, value_t *value);
    value_t *Value_map_get_name(value_t *map, const char *name);
    void Value_map_set_name(value_t *map, const char *name, value_t *value);
    value_t *Value_map_keys(value_t *map);

    // Lire un élément d'énumération : Couleur.ROUGE, .name et .value. NULL s'il n'existe pas.
    value_t *Value_enum_get(value_t *enumeration, const char *name);
    value_t *Value_enum_item_name(value_t *item);
    value_t *Value_enum_item_value(value_t *item);
    // Retrouve l'élément qui a cette valeur, un number ou une str. NULL s'il n'existe pas.
    value_t *Value_enum_find(value_t *enumeration, value_t *value);

    // Questions sur une valeur
    bool Value_is_true(value_t *value);
    bool Value_equals(value_t *left, value_t *right);
    double Value_as_double(value_t *value);
    char Value_number_to_char(double number);
    int Value_compare_text(value_t *left, value_t *right, bool *ok);
    const char *Value_type_name(value_t *value);

    // Texte d'une valeur : le résultat doit être libéré avec Buffer_Free.
    void Value_to_buffer(struct VISITOR_STRUCT *visitor, buffer_t *buffer, value_t *value);
    void Value_print(struct VISITOR_STRUCT *visitor, FILE *file, value_t *value);
    value_t *Value_to_string(struct VISITOR_STRUCT *visitor, value_t *value);

    // Vérifie le nombre d'arguments d'une fonction native.
    void Value_check_args(ast_t *node, const char *name, int argc, int expected);
    // Vérifie le type d'un argument d'une fonction native.
    void Value_check_type(ast_t *node, const char *name, value_t *value, type_ceflo expected);

    // Méthodes de list, map et str : retourne NULL si la méthode n'existe pas.
    value_t *Value_call_method(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, const char *name, value_t **args, int argc);

#endif
