/**
 * @file value.h
 * @brief une valeur de la machine virtuelle.
 *
 * [NOTE]
 *      Une valeur tient dans 16 octets et se copie comme un int : none, bool,
 *      char et number ne demandent aucune allocation. Tout le reste (chaînes,
 *      listes, objets...) est un `Obj` alloué par le garbage collector, et la
 *      valeur n'en garde que l'adresse : voir object.h.
 *
 *      C'est ce qui rend la VM rapide : `i += 1` ne crée plus aucun objet.
 */

#ifndef __VM_VALUE_H__
#define __VM_VALUE_H__

    #include <stdbool.h>
    #include <stdint.h>
    #include <stddef.h>

    typedef struct Obj Obj;
    typedef struct ObjString ObjString;

    typedef enum{
        VAL_NONE,
        VAL_BOOL,
        VAL_CHAR,
        VAL_NUMBER,
        VAL_OBJ,
        // Jamais visible par un script : une variable globale pas encore définie,
        // une case vide d'un map. Lire une globale UNDEFINED est une erreur.
        VAL_UNDEFINED,
    }ValueType;

    typedef struct{
        ValueType type;
        union{
            bool boolean;
            char character;
            double number;
            Obj *obj;
        }as;
    }Value;

    #define NONE_VAL            ((Value){VAL_NONE,{.number = 0}})
    #define UNDEFINED_VAL       ((Value){VAL_UNDEFINED,{.number = 0}})
    #define BOOL_VAL(b)         ((Value){VAL_BOOL,{.boolean = (b)}})
    #define CHAR_VAL(c)         ((Value){VAL_CHAR,{.character = (c)}})
    #define NUMBER_VAL(n)       ((Value){VAL_NUMBER,{.number = (n)}})
    #define OBJ_VAL(o)          ((Value){VAL_OBJ,{.obj = (Obj*)(o)}})

    #define IS_NONE(v)          ((v).type == VAL_NONE)
    #define IS_UNDEFINED(v)     ((v).type == VAL_UNDEFINED)
    #define IS_BOOL(v)          ((v).type == VAL_BOOL)
    #define IS_CHAR(v)          ((v).type == VAL_CHAR)
    #define IS_NUMBER(v)        ((v).type == VAL_NUMBER)
    #define IS_OBJ(v)           ((v).type == VAL_OBJ)

    #define AS_BOOL(v)          ((v).as.boolean)
    #define AS_CHAR(v)          ((v).as.character)
    #define AS_NUMBER(v)        ((v).as.number)
    #define AS_OBJ(v)           ((v).as.obj)

    // Un type écrit avec `::` : les types de base se testent sans chercher de nom.
    typedef enum{
        TYPE_ANY,           // pas de `::`
        TYPE_INT,
        TYPE_FLOAT,         // float et number
        TYPE_BOOL,
        TYPE_CHAR,
        TYPE_STR,           // str et string
        TYPE_LIST,
        TYPE_MAP,
        TYPE_NAMED,         // une structure ou une énumération : cherchée par son nom
    }TypeKind;

    typedef struct{
        TypeKind kind;
        ObjString *name;    // le nom écrit dans le code, pour les messages ; NULL avec TYPE_ANY
    }TypeRef;

    // Un tableau de valeurs qui grandit : les constantes d'un chunk, par exemple.
    typedef struct{
        Value *values;
        int count;
        int capacity;
    }ValueArray;

    void value_array_init(ValueArray *array);
    void value_array_write(ValueArray *array, Value value);
    void value_array_free(ValueArray *array);

    // Les règles de CeFlo sur les valeurs. Ce sont celles de l'interpréteur
    // d'origine (src/Value.c, src/value_expr.c, src/typer.c) : il fait foi.

    // false, none, 0, '\0', "" et une liste vide sont faux.
    bool value_is_true(Value value);
    // `==` : par valeur pour none, bool, char, number et str ; par adresse sinon.
    bool value_equals(Value a, Value b);
    // none, bool, char, number, str, list, map, object, func, struct, enum, generic
    const char *value_type_name(Value value);
    // Comme value_type_name, mais un number entier s'appelle int et les autres float.
    const char *value_typer_name(Value value);

#endif
