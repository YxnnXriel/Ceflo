/**
 * @file chunk.h
 * @brief le bytecode : la suite d'instructions d'une fonction.
 *
 * [NOTE]
 *      La VM est une machine à pile. Chaque instruction prend ses opérandes au
 *      sommet de la pile et y pose son résultat. `a + 1` devient :
 *
 *          OP_GET_LOCAL 0      pile : a
 *          OP_CONSTANT  3      pile : a 1
 *          OP_ADD              pile : a+1
 *
 *      Une instruction fait un octet, suivi de ses opérandes. Un opérande u8
 *      fait un octet, un u16 en fait deux (poids fort d'abord).
 *
 *      Pour chaque octet du code, `nodes` garde le noeud de l'AST qui l'a
 *      produit : une erreur s'affiche donc exactement comme avant, avec la
 *      ligne et la colonne du code source.
 *
 *      Dans les commentaires ci-dessous, [avant] -> [après] décrit le sommet de
 *      la pile (le sommet est à droite).
 */

#ifndef __VM_CHUNK_H__
#define __VM_CHUNK_H__

    #include "value.h"
    #include "ast.h"

    typedef enum{
        // ======== valeurs ========
        OP_CONSTANT,        // u16 k           [] -> [constants[k]]
        OP_NONE,            //                 [] -> [none]
        OP_TRUE,            //                 [] -> [true]
        OP_FALSE,           //                 [] -> [false]
        OP_POP,             //                 [a] -> []
        OP_DUP,             //                 [a] -> [a a]
        OP_DUP2,            //                 [a b] -> [a b a b]

        // ======== variables ========
        OP_GET_LOCAL,       // u8 slot         [] -> [slots[slot]]
        OP_SET_LOCAL,       // u8 slot         [a] -> [a]          slots[slot] = a
        OP_GET_UPVALUE,     // u8 i            [] -> [*upvalues[i]]
        OP_SET_UPVALUE,     // u8 i            [a] -> [a]
        OP_GET_GLOBAL,      // u16 i           [] -> [globals[i]]  UNDEFINED : builtin du même nom, sinon erreur
        OP_SET_GLOBAL,      // u16 i           [a] -> [a]          erreur si la globale n'est pas définie
        OP_DEFINE_GLOBAL,   // u16 i           [a] -> []           `var`, `func`, `struct`... au niveau du fichier
        OP_GET_BUILTIN,     // u16 k           [] -> [builtin]     k : constante, le nom du builtin
        OP_CLOSE_UPVALUE,   //                 [a] -> []           la variable capturée quitte la pile

        // ======== vérifications de type ========
        OP_CHECK_TYPE,      // u16 c           [a] -> [a]          erreur si a n'a pas le type chunk->checks[c]

        // ======== opérateurs ========
        // Un nombre avec un nombre est le cas rapide ; tout le reste suit
        // vm_binary (les règles de src/value_expr.c).
        OP_ADD,             //                 [a b] -> [a+b]
        OP_SUBTRACT,        //                 [a b] -> [a-b]
        OP_MULTIPLY,        //                 [a b] -> [a*b]
        OP_DIVIDE,          //                 [a b] -> [a/b]
        OP_MODULO,          //                 [a b] -> [a%b]
        OP_EQUAL,           //                 [a b] -> [a==b]
        OP_NOT_EQUAL,       //                 [a b] -> [a!=b]
        OP_LESS,            //                 [a b] -> [a<b]
        OP_LESS_EQUAL,      //                 [a b] -> [a<=b]
        OP_GREATER,         //                 [a b] -> [a>b]
        OP_GREATER_EQUAL,   //                 [a b] -> [a>=b]
        OP_NEGATE,          //                 [a] -> [-a]
        OP_NOT,             //                 [a] -> [!a]
        OP_TO_BOOL,         //                 [a] -> [bool(a)]    la fin de `and` et `or`

        // ======== sauts ========
        // Le décalage se compte depuis l'octet qui suit l'instruction.
        OP_JUMP,            // u16 d           ip += d
        OP_JUMP_IF_FALSE,   // u16 d           [a] -> []           si a est faux : ip += d
        OP_LOOP,            // u16 d           ip -= d             (un point sûr pour le GC)

        // ======== fonctions ========
        OP_CALL,            // u8 n            [f a1..an] -> [résultat]
        OP_INVOKE,          // u16 nom, u8 n   [o a1..an] -> [résultat]     o.nom(a1..an)
        OP_CLOSURE,         // u16 k, puis pour chaque upvalue : u8 is_local, u8 index
                            //                 [] -> [closure de la fonction constants[k]]
        OP_RETURN,          //                 [a] -> retourne a à l'appelant

        // ======== list, map, accès ========
        OP_LIST,            // u16 n           [a1..an] -> [[a1..an]]
        OP_MAP,             // u16 n           [k1 v1..kn vn] -> [{k1: v1..}]
        OP_GET_INDEX,       //                 [c i] -> [c[i]]
        OP_SET_INDEX,       // u16 d           [c i v] -> []       list, map : range v, puis ip += d
                            //                 str : [nouvelle chaîne], et le code qui suit la range à la place de c
        OP_GET_FIELD,       // u16 nom         [o] -> [o.nom]
        OP_SET_FIELD,       // u16 nom         [o v] -> [v]
        OP_FORMAT,          // u8 n            [a1..an] -> [str]   le texte de chaque valeur, mis bout à bout

        // ======== structures et énumérations ========
        OP_STRUCT,          // u16 k           [] -> [structure]    k : constante, la description (voir compiler.h)
        OP_METHOD,          // u16 nom         [s closure] -> [s]   range une méthode (et __init__, __str__, __cast__)
        OP_FIELDS_INIT,     //                 [s closure] -> [s]   la closure qui initialise les attributs
        OP_NEW,             // u16 nom, u8 n   [s a1..an] -> [objet]    `new Nom(...)` ; nom pour les erreurs
        OP_TYPE_CALL,       // u16 nom, u8 n   [a1..an] -> [résultat]   `@nom(...)`
        OP_ENUM,            // u16 k           [] -> [énumération]  k : constante, la description

        // ======== boucles for ... in et match ========
        // Le parcours occupe trois cases cachées : c, les clés relevées (un map) ou none, et i.
        OP_ITER_PREP,       //                 [c] -> [c clés 0]    prépare le parcours de c (list, map, str)
        OP_ITER_NEXT,       // u8 n, u16 d     [c clés i] -> [c clés i+1 x] (n=1) ou [c clés i+1 x y] (n=2)
                            //                 list, str : x l'élément, ou x l'index et y l'élément
                            //                 map : x la clé, ou x la clé et y la valeur
                            //                 si le parcours est fini : [c clés i] et ip += d
        OP_MATCH_FAIL,      // u8 has_enum     [sujet] -> []        aucun `case` et pas de `else`

        // ======== modules ========
        OP_IMPORT,          // u16 chemin      [] -> [module]       charge (une fois) et exécute le fichier
        OP_IMPORT_NAME,     // u16 nom         [module] -> [module]  définit la globale `nom` du fichier courant
                            //                 (valeur, type et constance du module importé)
        OP_IMPORT_VALUE,    // u16 nom         [module] -> [valeur]  un import dans un bloc : la valeur devient une locale
        OP_IMPORT_ALL,      //                 [module] -> []       copie les noms exportés dans le fichier courant
        OP_IMPORT_NATIVE,   // u16 chemin, u16 nom   [] -> [native]  une fonction d'une bibliothèque .so
        OP_EXPORT,          // u16 nom         [] -> []             le fichier publie ce nom

        // ======== instructions rapides ========
        // Des raccourcis pour les cas fréquents : le compilateur les choisit quand ils sont sûrs.
        OP_SET_LOCAL_POP,   // u8 slot         [a] -> []           slots[slot] = a
        OP_ADD_LOCAL,       // u8 slot, u16 k  [] -> []            slots[slot] = slots[slot] + constants[k]   (x += 1)
        OP_GET_SELF_FIELD,  // u8 i            [] -> [self.fields[i]]   dans une méthode : self est slots[0]
        OP_SET_SELF_FIELD,  // u8 i            [v] -> []           sans vérification : le compilateur s'en charge

        OP_COUNT,
    }OpCode;

    // Ce que vérifie un OP_CHECK_TYPE, et le message s'il échoue.
    typedef enum{
        CHECK_VARIABLE,     // "`x` est de type int, pas str"
        CHECK_ARGUMENT,     // même message, pour un paramètre
        CHECK_RETURN,       // "`f` doit retourner une valeur de type int, pas str"
    }CheckKind;

    typedef struct{
        CheckKind kind;
        TypeRef type;
        ObjString *subject;     // le nom de la variable, du paramètre ou de la fonction
    }TypeCheck;

    typedef struct{
        int count;
        int capacity;
        uint8_t *code;
        ast_t **nodes;          // le noeud de chaque octet de code
        ValueArray constants;
        TypeCheck *checks;
        int check_count;
        int check_capacity;
    }Chunk;

    void chunk_init(Chunk *chunk);
    void chunk_free(Chunk *chunk);
    void chunk_write(Chunk *chunk, uint8_t byte, ast_t *node);
    // Retourne le numéro de la constante. Une même chaîne n'est rangée qu'une fois.
    int chunk_add_constant(Chunk *chunk, Value value);
    int chunk_add_check(Chunk *chunk, TypeCheck check);

    // Le nom d'une instruction, pour le désassembleur.
    const char *opcode_name(OpCode op);

    // Affiche le bytecode d'une fonction : `ceflo --dis fichier.cf`.
    void chunk_disassemble(Chunk *chunk, const char *name);
    int chunk_disassemble_instruction(Chunk *chunk, int offset);

#endif
