/**
 * @file object.h
 * @brief tout ce que le garbage collector alloue : chaînes, listes, maps,
 *        fonctions, structures, objets, énumérations, modules.
 *
 * [NOTE]
 *      Chaque objet commence par un `Obj` : son type, sa marque pour le GC, et
 *      le lien vers l'objet alloué juste avant lui. Le GC parcourt cette liste
 *      pour libérer ce qui n'est plus atteignable (memory.h).
 *
 *      Les noms et les constantes du code sont internés : deux noms de même
 *      texte sont le même objet, et les comparer revient à comparer deux
 *      adresses. Un long texte construit pendant l'exécution ne l'est pas :
 *      le hacher à chaque `texte += ligne` rendait la boucle quadratique.
 */

#ifndef __VM_OBJECT_H__
#define __VM_OBJECT_H__

    #include "value.h"
    #include "chunk.h"
    #include "ast.h"

    struct VM;

    typedef enum{
        OBJ_STRING,
        OBJ_LIST,
        OBJ_MAP,
        OBJ_FUNCTION,       // le code d'une fonction, sans ses variables capturées
        OBJ_CLOSURE,        // une fonction et les variables qu'elle a capturées
        OBJ_UPVALUE,        // une variable capturée par une closure
        OBJ_NATIVE,         // une fonction écrite en C
        OBJ_BOUND_METHOD,   // `p.avance` sans parenthèses : la méthode et son objet
        OBJ_STRUCT,
        OBJ_INSTANCE,
        OBJ_ENUM,
        OBJ_ENUM_VALUE,
        OBJ_MODULE,
        OBJ_GENERIC,        // un pointeur C pour les bibliothèques natives (un FILE*...)
    }ObjType;

    struct Obj{
        ObjType type;
        bool marked;
        bool printing;   // en cours d'affichage : une liste qui se contient s'écrit "..."
        struct Obj *next;
    };

    #define OBJ_TYPE(v)         (AS_OBJ(v)->type)
    #define IS_OBJ_TYPE(v,t)    (IS_OBJ(v) && OBJ_TYPE(v) == (t))

    #define IS_STRING(v)        IS_OBJ_TYPE(v,OBJ_STRING)
    #define IS_LIST(v)          IS_OBJ_TYPE(v,OBJ_LIST)
    #define IS_MAP(v)           IS_OBJ_TYPE(v,OBJ_MAP)
    #define IS_FUNCTION(v)      IS_OBJ_TYPE(v,OBJ_FUNCTION)
    #define IS_CLOSURE(v)       IS_OBJ_TYPE(v,OBJ_CLOSURE)
    #define IS_NATIVE(v)        IS_OBJ_TYPE(v,OBJ_NATIVE)
    #define IS_BOUND_METHOD(v)  IS_OBJ_TYPE(v,OBJ_BOUND_METHOD)
    #define IS_STRUCT(v)        IS_OBJ_TYPE(v,OBJ_STRUCT)
    #define IS_INSTANCE(v)      IS_OBJ_TYPE(v,OBJ_INSTANCE)
    #define IS_ENUM(v)          IS_OBJ_TYPE(v,OBJ_ENUM)
    #define IS_ENUM_VALUE(v)    IS_OBJ_TYPE(v,OBJ_ENUM_VALUE)
    #define IS_MODULE(v)        IS_OBJ_TYPE(v,OBJ_MODULE)
    #define IS_GENERIC(v)       IS_OBJ_TYPE(v,OBJ_GENERIC)

    #define AS_STRING(v)        ((ObjString*)AS_OBJ(v))
    #define AS_CSTRING(v)       (((ObjString*)AS_OBJ(v))->chars)
    #define AS_LIST(v)          ((ObjList*)AS_OBJ(v))
    #define AS_MAP(v)           ((ObjMap*)AS_OBJ(v))
    #define AS_FUNCTION(v)      ((ObjFunction*)AS_OBJ(v))
    #define AS_CLOSURE(v)       ((ObjClosure*)AS_OBJ(v))
    #define AS_NATIVE(v)        ((ObjNative*)AS_OBJ(v))
    #define AS_BOUND_METHOD(v)  ((ObjBoundMethod*)AS_OBJ(v))
    #define AS_STRUCT(v)        ((ObjStruct*)AS_OBJ(v))
    #define AS_INSTANCE(v)      ((ObjInstance*)AS_OBJ(v))
    #define AS_ENUM(v)          ((ObjEnum*)AS_OBJ(v))
    #define AS_ENUM_VALUE(v)    ((ObjEnumValue*)AS_OBJ(v))
    #define AS_MODULE(v)        ((ObjModule*)AS_OBJ(v))
    #define AS_GENERIC(v)       ((ObjGeneric*)AS_OBJ(v))


    //  ===================================== chaînes =====================================

    // Le texte se termine toujours par '\0', mais peut aussi en contenir.
    struct ObjString{
        Obj obj;
        int length;
        uint32_t hash;      // pas encore calculé pour un long texte : string_get_hash
        bool interned;
        char chars[];
    };

    // Les noms : copie le texte, ou retourne la chaîne internée qui a ce texte.
    // Les clés d'une Table viennent toujours d'ici.
    ObjString *string_copy(struct VM *vm, const char *chars, int length);
    ObjString *string_from(struct VM *vm, const char *chars);
    // Un texte construit pendant l'exécution (concaténation, format, input, split...) :
    // interné s'il est court, sinon une nouvelle chaîne dont le hash attend d'être utile.
    ObjString *string_new(struct VM *vm, const char *chars, int length);
    // Une chaîne neuve, non internée, de `length` octets que l'appelant remplit
    // (chars[length] vaut déjà '\0'). Pour construire un long texte sans copie :
    // un texte court passe plutôt par string_new, qui l'interne.
    ObjString *string_reserve(struct VM *vm, int length);
    uint32_t string_hash(const char *chars, int length);
    uint32_t string_get_hash(ObjString *string);
    // Même texte : la même adresse, ou la même longueur et les mêmes octets.
    bool string_equals(ObjString *a, ObjString *b);


    //  ===================================== table =====================================

    // Table de hachage dont les clés sont des chaînes internées (string_copy) : les noms des
    // globales, des méthodes, des builtins. Ce n'est pas le `map` de CeFlo.
    typedef struct{
        ObjString *key;     // NULL : case vide (ou effacée si value n'est pas UNDEFINED)
        Value value;
    }Entry;

    typedef struct{
        int count;
        int capacity;
        Entry *entries;
    }Table;

    void table_init(Table *table);
    void table_free(Table *table);
    bool table_get(Table *table, ObjString *key, Value *value);
    // Retourne true si la clé n'existait pas encore.
    bool table_set(Table *table, ObjString *key, Value value);
    bool table_delete(Table *table, ObjString *key);
    ObjString *table_find_string(Table *table, const char *chars, int length, uint32_t hash);


    //  ===================================== list et map =====================================

    typedef struct{
        Obj obj;
        Value *items;
        int count;
        int capacity;
    }ObjList;

    ObjList *list_new(struct VM *vm);
    void list_append(struct VM *vm, ObjList *list, Value value);

    /*
        Le `map` de CeFlo : il garde l'ordre d'insertion, comme le Map de
        TypeScript. Les clés sont des str, des number, des char ou des éléments
        d'énumération (comparés par adresse). Même principe que src/map.c :

            entries : les paires (clé, valeur), dans l'ordre d'arrivée
            index   : pour un hash, la position de sa paire dans entries

        Une clé enlevée laisse un trou (key UNDEFINED) jusqu'à la reconstruction.
    */
    typedef struct{
        Value key;
        Value value;
        uint32_t hash;
    }MapEntry;

    typedef struct{
        Obj obj;
        MapEntry *entries;
        int32_t *index;
        int size;       // les cases utilisées dans entries, trous compris
        int count;      // les clés vivantes
        int capacity;
    }ObjMap;

    ObjMap *map_new(struct VM *vm);
    // Une clé d'un autre type (une liste...) est une erreur du script : false.
    bool map_key_is_valid(Value key);
    // false si la clé n'existe pas.
    bool map_get(ObjMap *map, Value key, Value *value);
    void map_set(struct VM *vm, ObjMap *map, Value key, Value value);
    bool map_remove(ObjMap *map, Value key);


    //  ===================================== fonctions =====================================

    typedef struct ObjModule ObjModule;

    typedef struct{
        Obj obj;
        int arity;
        int upvalue_count;
        Chunk chunk;
        ObjString *name;        // NULL pour le code d'un fichier
        ObjModule *module;      // les globales que ce code lit et écrit
        ast_t *definition;      // pour situer les erreurs ; reste en vie jusqu'à la fin
        TypeRef *param_types;   // arity cases
        TypeRef return_type;
        bool is_method;         // la case 0 de la pile est `self`
    }ObjFunction;

    ObjFunction *function_new(struct VM *vm, ObjModule *module);

    // Une variable capturée. Tant que sa fonction tourne, elle vit sur la pile
    // (`location` pointe dans la pile) ; ensuite elle est copiée dans `closed`.
    typedef struct ObjUpvalue{
        Obj obj;
        Value *location;
        Value closed;
        struct ObjUpvalue *next;    // les upvalues encore ouvertes, triées par adresse
    }ObjUpvalue;

    typedef struct{
        Obj obj;
        ObjFunction *function;
        ObjUpvalue **upvalues;
        int upvalue_count;
    }ObjClosure;

    ObjClosure *closure_new(struct VM *vm, ObjFunction *function);
    ObjUpvalue *upvalue_new(struct VM *vm, Value *slot);

    // Une fonction C. Elle reçoit ses arguments et retourne sa valeur.
    // Une erreur s'écrit vm_error(vm, ...) : le programme s'arrête.
    typedef Value (*NativeFn)(struct VM *vm, int argc, Value *args);

    typedef struct{
        Obj obj;
        NativeFn function;
        const char *name;   // un texte qui vit jusqu'à la fin du programme
    }ObjNative;

    ObjNative *native_new(struct VM *vm, const char *name, NativeFn function);

    typedef struct{
        Obj obj;
        Value receiver;
        Value method;       // une closure ou une native
    }ObjBoundMethod;

    ObjBoundMethod *bound_method_new(struct VM *vm, Value receiver, Value method);


    //  ===================================== structures et objets =====================================

    /*
        Une structure connaît ses attributs dès la compilation : un objet garde
        donc ses valeurs dans un simple tableau, et `p.x` est la case 0 si x est
        le premier attribut.
    */
    typedef struct{
        Obj obj;
        ObjString *name;
        ast_t *definition;
        int field_count;
        ObjString **field_names;
        TypeRef *field_types;
        bool *field_is_const;   // `const` dans la structure : « `K` est une constante »
        Value fields_init;      // une closure qui donne leur valeur de départ aux attributs, ou NONE
        Table methods;          // nom -> closure ; __init__ est rangé à part
        Value inits[8];         // __init__ selon son nombre de paramètres (0 à 7), ou NONE
        Value str_method;       // __str__, ou NONE
        Value cast_method;      // __cast__, ou NONE
    }ObjStruct;

    ObjStruct *struct_new(struct VM *vm, ObjString *name, ast_t *definition, int field_count);
    // La case d'un attribut, ou -1.
    int struct_field_index(ObjStruct *structure, ObjString *name);

    typedef struct{
        Obj obj;
        ObjStruct *structure;
        Value fields[];
    }ObjInstance;

    ObjInstance *instance_new(struct VM *vm, ObjStruct *structure);


    //  ===================================== énumérations =====================================

    // Chaque élément n'existe qu'une fois : deux éléments se comparent par leur adresse.
    typedef struct{
        Obj obj;
        Value owner;        // l'ObjEnum
        ObjString *name;
        Value value;        // un number ou une str
    }ObjEnumValue;

    typedef struct{
        Obj obj;
        ObjString *name;
        ast_t *definition;
        int count;
        ObjEnumValue **items;   // dans l'ordre de la définition
    }ObjEnum;

    ObjEnum *enum_new(struct VM *vm, ObjString *name, ast_t *definition, int count);
    ObjEnumValue *enum_value_new(struct VM *vm, ObjEnum *owner, ObjString *name, Value value);


    //  ===================================== modules =====================================

    /*
        Un fichier CeFlo. Ses globales sont rangées par numéro : le compilateur
        traduit chaque nom en numéro, et OP_GET_GLOBAL lit simplement la case.
        `names` garde le nom de chaque case, pour les erreurs et pour `import`.
    */
    struct ObjModule{
        Obj obj;
        ObjString *path;
        ValueArray globals;         // UNDEFINED tant que la globale n'est pas définie
        ObjString **names;          // le nom de chaque globale
        TypeRef *types;             // le type de chaque globale (TYPE_ANY sans `::`)
        bool *is_const;
        int capacity;
        Table indexes;              // nom -> numéro (un NUMBER_VAL)
        ObjString **exports;        // NULL : tout est exporté
        int export_count;
        bool loaded;                // le code du fichier a fini de s'exécuter
    };

    ObjModule *module_new(struct VM *vm, ObjString *path);
    // Le numéro de la globale qui a ce nom ; la crée (UNDEFINED) si elle n'existe pas.
    int module_global_index(struct VM *vm, ObjModule *module, ObjString *name);


    //  ===================================== divers =====================================

    typedef struct{
        Obj obj;
        void *pointer;
    }ObjGeneric;

    ObjGeneric *generic_new(struct VM *vm, void *pointer);

#endif
