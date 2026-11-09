---
numero: 13
titre: structures et méthodes
note: des cases au lieu de noms
partie: Le bytecode
---

## 13.1 Un objet sans portée

Dans le visitor, un objet était une portée. `visitor_new_object` créait un
`scope_t` avec `newScope`, y définissait chaque attribut et chaque méthode, et
`p.x` cherchait la chaîne `"x"` dans sa table de hachage. Dans une méthode, la
portée de l'objet devenait la portée parente : `x` tout seul trouvait
l'attribut en remontant la chaîne, exactement comme une variable. C'était
simple, et c'est pour ça que `self` était facultatif sans une ligne de code en
plus.

Depuis le chapitre 10, on n'a plus de portées. On a des cases de pile pour les
locales, des cases de module pour les globales, et depuis le chapitre 12 des
upvalues pour les variables capturées. Il faut maintenant une case pour les
attributs. Voici le programme qu'on veut faire tourner, et le bytecode qu'on
veut obtenir pour sa méthode :

```ceflo fichier="point.cf"
struct Point {
    var x::int = 0
    var y = 0

    func avance(n) {
        x += n
        return self
    }
}

var p = new Point()
p.avance(2).avance(3)
println(p.x, " ", p.y)
```

```terminal
$ ./Ceflo --dis point.cf
...
== avance ==
0000     6  GET_SELF_FIELD        0
0002     |  GET_LOCAL             1
0004     |  ADD
0005     |  CHECK_TYPE            0  variable `x` :: int
0008     |  SET_SELF_FIELD        0
0010     7  GET_LOCAL             0
0012     |  RETURN
...
5 0
```

`x += n` est devenu « lis la case 0 de self, ajoute la case 1 de la pile,
vérifie que c'est un `int`, range dans la case 0 de self ». Le nom `x` a
disparu. Il n'y a ni table, ni chaîne, ni même de `self.` dans le code
source : le compilateur sait que `x` est le premier attribut de `Point`, et il
l'écrit. C'est la troisième des quatre idées de `docs/VM.md` : **les attributs
ont une place fixe**.

Pour y arriver, on va construire dans l'ordre :

1. deux objets dans `object.h`, la structure et l'instance (13.2) ;
2. les instructions qui créent une structure à l'exécution (13.3) ;
3. lire et écrire `p.x` depuis n'importe où (13.4) ;
4. `x` sans `self.` dans une méthode, et les instructions rapides (13.5) ;
5. `new`, les versions de `__init__`, `__str__` et `__cast__` (13.6) ;
6. appeler une méthode sans fabriquer d'objet intermédiaire (13.7) ;
7. les énumérations et `match` (13.8).

### Ce que font les autres

Le choix d'une place fixe n'est pas le seul possible, et il vaut la peine de
voir ce que font les autres avant de s'y engager.

En **Python**, chaque objet a un dictionnaire, `__dict__`, et `p.x` est une
recherche dans ce dictionnaire. On peut ajouter un attribut à tout moment :
`p.z = 3` marche sur n'importe quel objet. Le prix est la mémoire (un
dictionnaire par objet) et le temps (un hachage à chaque accès). Python a
d'ailleurs ajouté `__slots__`, qui donne des cases fixes à qui le demande, et
CPython 3.11 range désormais les valeurs à part du dictionnaire pour les
objets ordinaires.

En **Lua**, un objet est une table, et ses méthodes sont dans une autre table,
sa métatable. C'est encore plus souple, et c'est la même recherche par nom.

**JavaScript** est le cas le plus intéressant. Le langage permet tout, comme
Python, mais les moteurs comme V8 devinent la forme des objets pendant
l'exécution : deux objets créés de la même façon partagent une « classe
cachée », qui donne à chaque attribut une position. Un accès qui voit toujours
la même forme devient une lecture à une position fixe. C'est notre case fixe,
retrouvée à l'exécution au prix d'un moteur très compliqué.

En **C** et en **Java**, la forme d'un objet est connue à la compilation, et
`p.x` est une lecture à un décalage fixe. On ne peut rien ajouter.

CeFlo est entre les deux. Une structure déclare ses attributs avec `var`, le
compilateur les connaît, et on ne peut pas en ajouter : `p.z = 3` est une
erreur. Pour un objet dont les clés changent, il y a le `map`. Ce n'est pas une
contrainte qu'on découvre avec la VM : le visitor refusait déjà un attribut
inconnu. La VM en profite.

## 13.2 Deux objets : la structure et l'instance

On commence par les données. Dans `src/vm/object.h`, dans l'énumération
`ObjType`, on a déjà réservé deux lignes au chapitre 11 : `OBJ_STRUCT` et
`OBJ_INSTANCE`. Sous la section des fonctions, on ajoute les deux types :

```c fichier="src/vm/object.h" lignes="242-272" focus="251-252,266-270"
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
```

Les champs de `ObjStruct`, un par un :

- `name` sert aux messages (« `Point` n'a pas de __init__... ») et à
  `println` d'un objet sans `__str__`, qui écrit `<Point>`.
- `definition` est le noeud de l'AST, gardé pour situer une erreur.
- `field_count` et les trois tableaux `field_names`, `field_types`,
  `field_is_const` décrivent les attributs. Trois tableaux parallèles plutôt
  qu'un tableau de `struct` : le plus fréquent est de chercher un nom, et
  `field_names` se parcourt alors sans sauter par-dessus les types.
- `fields_init` est une closure qui calcule les valeurs de départ. On verra en
  13.3 pourquoi ce ne sont pas de simples constantes.
- `methods` est une `Table` (chapitre 11) : nom vers closure.
- `inits`, `str_method`, `cast_method` rangent à part les trois méthodes que
  la VM appelle elle-même. Elle n'a ainsi jamais à chercher `"__init__"` dans
  `methods`.

`ObjInstance` est bien plus petit. Il ne garde que ce qui change d'un objet à
l'autre : sa structure, et ses valeurs. `Value fields[]` est un **membre
flexible**, permis depuis le C99. Il n'a pas de taille dans le `struct` :
`sizeof(ObjInstance)` ne le compte pas, et on alloue l'objet avec la place des
valeurs juste derrière l'en-tête. Voici un `Point` en mémoire :

```text
ObjInstance de p (56 octets)                 ObjStruct Point (partagé)
+----------------------------+               +---------------------------+
| obj : type, marque, next   | 16 octets     | name        -> "Point"    |
| structure -----------------+-------------->| field_count  2            |
| fields[0] : x = 5          | 16 octets     | field_names -> ["x", "y"] |
| fields[1] : y = 0          | 16 octets     | field_types -> [int, any] |
+----------------------------+               | methods     -> {avance}   |
  (structure : 8 octets)                     +---------------------------+
```

Les tailles ne sont pas devinées : un petit programme C qui inclut
`object.h` et affiche `sizeof(ObjInstance)` et `offsetof(ObjInstance,fields)`
donne 24 et 24 sur ma machine. Les valeurs commencent juste après le
pointeur, et un `Point` fait 24 + 2 × 16 = 56 octets.

Un `Point` coûte une seule allocation. Dans le visitor, le même objet
demandait une portée, une table de hachage, une entrée par attribut et une par
méthode, plus un `value_t` alloué pour chaque nombre.

Dans `src/vm/object.c`, on écrit les trois fonctions. `struct_new` remplit
tous les champs avec des valeurs vides (lignes 158 à 183, sans surprise :
`NONE_VAL` partout, `TYPE_ANY` pour chaque attribut). Les deux autres
méritent qu'on s'y arrête :

```c fichier="src/vm/object.c" lignes="185-201"
// Les noms sont internés : comparer les adresses suffit.
int struct_field_index(ObjStruct *structure, ObjString *name){
    for(int i = 0; i < structure->field_count; i++){
        if(structure->field_names[i] == name)
            return i;
    }
    return -1;
}

ObjInstance *instance_new(VM *vm, ObjStruct *structure){
    size_t size = sizeof(ObjInstance) + sizeof(Value) * (size_t)structure->field_count;
    ObjInstance *instance = (ObjInstance*)vm_allocate_object(vm,size,OBJ_INSTANCE);
    instance->structure = structure;
    for(int i = 0; i < structure->field_count; i++)
        instance->fields[i] = NONE_VAL;
    return instance;
}
```

`struct_field_index` est une boucle, pas une table de hachage. Elle servira
quand le compilateur n'a pas pu trouver la case à l'avance, pour `p.x` écrit
hors d'une méthode, où il ne sait pas ce qu'est `p`. Une structure a rarement
plus de dix attributs, et comparer dix pointeurs coûte moins cher que hacher
un nom. Ce sont bien des pointeurs : grâce aux chaînes internées du chapitre
11, deux noms de même texte sont le même objet. Sans l'internement, cette
ligne serait un `memcmp`.

`instance_new` ne peut pas utiliser la macro `ALLOCATE_OBJ` des autres objets,
qui alloue `sizeof(type)`. Elle calcule la taille elle-même et appelle
`vm_allocate_object`. Les valeurs sont mises à `none` tout de suite : le GC
pourrait sinon lire des octets au hasard s'il marquait l'objet avant qu'on
l'ait rempli.

### Le GC doit les connaître

Un nouvel objet touche trois endroits de `src/vm/memory.c`. `object_size`
doit savoir sa taille, et pour une instance, elle dépend de sa structure :

```c fichier="src/vm/memory.c" lignes="70-72"
        case OBJ_STRUCT: return sizeof(ObjStruct);
        case OBJ_INSTANCE:
            return sizeof(ObjInstance) + sizeof(Value) * (size_t)((ObjInstance*)object)->structure->field_count;
```

Il y a ici un piège qu'on frôle sans le voir. Quand le GC libère une instance,
il lit `structure->field_count`. Si la structure est morte au même moment,
elle a peut-être déjà été libérée. Ce n'est pas le cas, grâce à l'ordre de
la liste `vm->objects` : chaque objet est ajouté **en tête** (`object->next =
vm->objects`), et `gc_sweep` la parcourt depuis la tête. Une instance est
toujours créée après sa structure, elle est donc plus près de la tête, et
libérée avant elle. Le commentaire de la ligne 133 de `memory.c` rappelle
qu'une instance a besoin de sa structure pour donner sa taille.

`gc_blacken` doit marquer ce que l'objet tient (lignes 240 à 258) : pour une
structure, son nom, ses noms d'attributs, ses types, `fields_init`, la table
des méthodes et les trois méthodes à part ; pour une instance, sa structure et
chacune de ses valeurs. Et `object_free` libère les trois tableaux et la table
d'une structure (lignes 106 à 113). Une instance n'a rien à libérer à part
elle-même : ses valeurs sont dans le même bloc.

À ce stade, `make` compile. Rien ne crée encore d'objet, mais le GC sait déjà
les suivre.

> [!note]
> Le `Value inits[8]` coûte 128 octets par structure, même pour une structure
> sans `__init__`. C'est un choix : il y a peu de structures et beaucoup
> d'objets, et un tableau se lit en une instruction.

## 13.3 Le modèle et sa copie

Une définition de structure n'est pas une déclaration passive : c'est du code
qui s'exécute, comme `var`. Voici le haut du bytecode de `point.cf` :

```terminal
$ ./Ceflo --dis point.cf
== point.cf ==
0000     1  STRUCT                0  <struct Point>
0003     5  CLOSURE               1  <func avance>
0006     |  METHOD                2  "avance"
0009     1  CLOSURE               3  <func Point>
0012     |  FIELDS_INIT
0013     |  DEFINE_GLOBAL         0
0016    11  CONSTANT              4  "Point"
0019     |  NEW                   4  "Point"  (0 args)
0023     |  DEFINE_GLOBAL         1
...
```

Et voici la pile pendant ces quatre instructions :

```text
                          pile après l'instruction
STRUCT 0                  [ Point ]
CLOSURE 1 <func avance>   [ Point  avance ]
METHOD "avance"           [ Point ]               avance rangée dans Point.methods
CLOSURE 3 <func Point>    [ Point  init_attributs ]
FIELDS_INIT               [ Point ]               rangée dans Point.fields_init
DEFINE_GLOBAL 0           [ ]                     globals[0] = Point
```

La structure reste au fond pendant qu'on lui accroche ses méthodes. Chaque
instruction de remplissage retire ce qu'elle range et laisse la structure en
place pour la suivante. C'est le même dessin que dans *Crafting Interpreters*
pour les classes de Lox, et c'est ce qui permet de compiler chaque méthode
comme n'importe quelle fonction : `compile_function` laisse sa closure au
sommet de la pile, et il suffit d'une instruction pour la ranger.

### Les instructions

Ajouter une instruction touche toujours les mêmes quatre endroits, qu'on a
vus au chapitre 11 : l'énumération `OpCode` de `chunk.h`, le nom et la forme
des opérandes dans `debug.c`, une étiquette et un `CASE` dans `vm.c`, et le
compilateur. Dans `src/vm/chunk.h`, dans la section des structures :

```c fichier="src/vm/chunk.h" lignes="95-101"
        // ======== structures et énumérations ========
        OP_STRUCT,          // u16 k           [] -> [structure]    k : constante, la description (voir compiler.h)
        OP_METHOD,          // u16 nom         [s closure] -> [s]   range une méthode (et __init__, __str__, __cast__)
        OP_FIELDS_INIT,     //                 [s closure] -> [s]   la closure qui initialise les attributs
        OP_NEW,             // u16 nom, u8 n   [s a1..an] -> [objet]    `new Nom(...)` ; nom pour les erreurs
        OP_TYPE_CALL,       // u16 nom, u8 n   [a1..an] -> [résultat]   `@nom(...)`
        OP_ENUM,            // u16 k           [] -> [énumération]  k : constante, la description
```

Dans `debug.c`, `OP_STRUCT` et `OP_METHOD` vont avec les instructions à un
opérand constant de deux octets (ligne 218), `OP_NEW` avec `OP_INVOKE`, qui
affichent un nom et un nombre d'arguments (ligne 254). `OP_FIELDS_INIT` n'a
pas d'opérande et tombe dans le cas par défaut.

### Le modèle

La constante 0, affichée `<struct Point>`, est un **modèle** : un `ObjStruct`
que le compilateur construit pendant la compilation, avec les noms, les types
et la constance des attributs, mais sans aucune méthode. Le compilateur le
fait dans `build_struct_model`, en deux passes. La première relève les noms
des attributs, sans doublon, dans l'ordre où ils sont écrits :

```c fichier="src/vm/compiler.c" lignes="745-763" fonction="build_struct_model"
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
```

La boucle intérieure suit `var->left` parce que `var a = 1, b = 2` est une
chaîne de noeuds (chapitre 3). Un nom déjà vu est sauté : `var x` écrit deux
fois dans une structure ne donne qu'un attribut, comme une variable déclarée
deux fois dans une portée garde sa case. Il faut connaître le nombre
d'attributs avant d'allouer le modèle, d'où le tableau `names` sur la pile du
C. La seconde passe remplit le modèle :

```c fichier="src/vm/compiler.c" lignes="765-777" fonction="build_struct_model"
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
```

C'est ici que `x` devient la case 0 et `y` la case 1, une fois pour toutes.
`merge_type` est la fonction du chapitre 10 : un attribut typé garde son type
s'il est redéclaré, et un type différent est une erreur.

### Pourquoi une copie

Pourquoi un modèle, et pas directement la structure ? On pourrait mettre la
structure finie dans les constantes, avec ses méthodes, et la pousser telle
quelle. Ça marcherait pour `point.cf`. Ça casse ici :

```ceflo fichier="usine.cf"
func Usine(depart) {
    struct Boite {
        var v = depart
    }
    return new Boite()
}

var a = Usine(1)
var b = Usine(2)
println(a.v, " ", b.v, " ", @bool(a == b))
var p::Boite = a
```

```terminal
$ ./Ceflo usine.cf
1 2 false
erreur: le type `Boite` n'existe pas
  --> usine.cf:11:5
   11 | var p::Boite = a
      |     ^
```

`var v = depart` lit un paramètre de `Usine`. La valeur de départ de `v` est
différente à chaque appel, et la closure qui la calcule aussi : elle capture
`depart` comme une upvalue, et une upvalue n'existe qu'à l'exécution
(chapitre 12). Les méthodes peuvent faire de même. Chaque appel de `Usine`
doit donc avoir sa `Boite`, avec ses propres closures. Le désassemblage le
montre :

```terminal
$ ./Ceflo --dis usine.cf
...
== Usine ==
0000     2  STRUCT                0  <struct Boite>
0003     |  CLOSURE               1  <func Boite>
0006      |                       local 1
0008     |  FIELDS_INIT
0009     5  GET_LOCAL             2
0011     |  NEW                   2  "Boite"  (0 args)
0015     |  RETURN

== Boite ==
0000     3  GET_LOCAL             0
0002     |  GET_UPVALUE           0
0004     |  SET_FIELD             0  "v"
0007     |  POP
0008     2  NONE
0009     |  RETURN
...
```

La closure des attributs capture la case 1 de `Usine`, c'est-à-dire `depart`.
`OP_STRUCT` fabrique donc une structure neuve à chaque exécution, en recopiant
la partie fixe du modèle. Dans `src/vm/vm.c`, dans la section des objets :

```c fichier="src/vm/vm.c" lignes="251-260" fonction="new_struct"
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
```

On copie les tableaux, pas les pointeurs vers les tableaux du modèle : le GC
libère les tableaux d'une structure avec elle, et deux structures qui
partageraient le même tableau le libéreraient deux fois.

La dernière ligne de `usine.cf` montre l'autre conséquence : `Boite` est une
locale de `Usine`, et un type écrit après `::` se cherche dans les globales du
fichier (une des différences voulues de `docs/VM.md`). En dehors de `Usine`,
`Boite` n'existe pas.

### Le compilateur

Le reste de la définition tient dans une fonction, qu'on ajoute dans la
section `$STRUCTURES` de `compiler.c` :

```c fichier="src/vm/compiler.c" lignes="816-846" fonction="compile_struct_definition" focus="823,835-836,839-842"
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
```

Ligne par ligne :

- `check_new_type` refuse une structure déjà définie dans la même portée. C'est
  une erreur de compilation, plus tôt que dans le visitor.
- `declare_early` est celle du chapitre 10 : une structure locale reçoit sa
  case avant son code, pour qu'une méthode puisse écrire `new Boite()`.
- Chaque méthode est compilée avec `FUNC_METHOD` et un `StructInfo`, le
  modèle et sa définition. C'est ce qui lui permettra de voir les attributs
  sans `self.` (section 13.5).
- `__cast__` est compilé comme une fonction ordinaire, sans `StructInfo` : il
  reçoit une valeur à convertir et s'appelle sans objet.
- `compile_fields_init` compile toutes les valeurs de départ, dans l'ordre,
  dans une méthode sans paramètre : c'est la fonction `Point` du
  désassemblage. Une structure sans attribut n'en a pas.

```c fichier="src/vm/compiler.c" lignes="792-808" fonction="compile_fields_init"
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
```

Pourquoi une closure, et pas une liste de constantes ? Parce qu'une valeur de
départ est une expression quelconque : `var v = depart`, `var liste = []`,
`var id = prochain_id()`. Python a fait le même choix en sens inverse : une
valeur écrite dans le corps d'une classe est calculée **une fois**, et
partagée par tous les objets. C'est le piège classique de `class A: l = []`,
où tous les objets partagent la même liste. Dans CeFlo, chaque `new` rappelle
la closure, et chaque objet a sa liste.

Côté VM, les trois instructions de remplissage sont courtes :

```c fichier="src/vm/vm.c" lignes="975-987"
    CASE(OP_STRUCT): PUSH(new_struct(vm,AS_STRUCT(READ_CONSTANT()))); NEXT();
    CASE(OP_ENUM): PUSH(new_enum(vm,AS_ENUM(READ_CONSTANT()))); NEXT();
    CASE(OP_METHOD):{
        ObjString *name = READ_STRING();
        Value method = POP();
        define_method(vm,AS_STRUCT(PEEK(0)),name,method);
        NEXT();
    }
    CASE(OP_FIELDS_INIT):{
        Value closure = POP();
        AS_STRUCT(PEEK(0))->fields_init = closure;
        NEXT();
    }
```

Et `define_method` trie les méthodes en les rangeant :

```c fichier="src/vm/vm.c" lignes="270-279" fonction="define_method"
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
```

Les trois noms spéciaux sont comparés par adresse : `vm_init` les a internés
une fois pour toutes. `__init__` est rangé selon son nombre de paramètres,
dans le tableau de huit cases. C'est pour ça que le compilateur refuse un
`__init__` de plus de 7 paramètres : il n'aurait pas de case, et
`inits[arity]` écrirait hors du tableau.

On peut compiler et lancer une structure sans méthode ni `new`. Elle se
définit, et `println(Point)` l'affiche.

## 13.4 Lire et écrire un attribut

Hors d'une méthode, le compilateur ne sait pas ce qu'est `p`. Ce pourrait être
un `Point`, un `map`, une énumération. `p.x` se compile donc avec le nom :

```ceflo fichier="champ.cf"
struct Point {
    var x::int = 0
    var y = 0
}
var p = new Point()
p.y = "haut"
p.x += 4
println(p.x, " ", p.y, " ", p)
p.x = 1.5
```

```terminal
$ ./Ceflo champ.cf
4 haut <Point>
erreur: `x` est de type int, pas float
  --> champ.cf:9:3
    9 | p.x = 1.5
      |   ^
$ ./Ceflo --dis champ.cf
...
0020     6  GET_GLOBAL            1
0023     |  CONSTANT              4  "haut"
0026     |  SET_FIELD             3  "y"
0029     |  POP
0030     7  GET_GLOBAL            1
0033     |  DUP
0034     |  GET_FIELD             5  "x"
0037     |  CONSTANT              6  4
0040     |  ADD
0041     |  SET_FIELD             5  "x"
0044     |  POP
...
```

`p.x += 4` est le cas le plus instructif. La cible `p` ne doit être calculée
qu'une fois (c'est la règle de `compile_assignement`, chapitre 10), mais il en
faut deux exemplaires : un pour lire `x`, un pour écrire. D'où le `DUP` :

```text
                    pile
GET_GLOBAL 1        [ p ]
DUP                 [ p  p ]
GET_FIELD "x"       [ p  0 ]
CONSTANT 4          [ p  0  4 ]
ADD                 [ p  4 ]
SET_FIELD "x"       [ 4 ]          p.x = 4
POP                 [ ]
```

`SET_FIELD` laisse la valeur rangée sur la pile, et le compilateur la retire
aussitôt. C'est l'héritage d'un langage où une affectation est une expression
(`a = b = 3` en C). Dans CeFlo, une affectation est une instruction, et ce
`POP` est toujours là. On aurait pu faire de `SET_FIELD` une instruction qui
ne laisse rien. On a gardé la forme de `OP_SET_LOCAL` et `OP_SET_GLOBAL` pour
que les trois se ressemblent.

Les deux instructions, dans `vm.c` :

```c fichier="src/vm/vm.c" lignes="934-947"
    CASE(OP_GET_FIELD):{
        ObjString *name = READ_STRING();
        Value object = PEEK(0);
        if(IS_INSTANCE(object)){
            int index = struct_field_index(AS_INSTANCE(object)->structure,name);
            if(index >= 0){
                vm->stack_top[-1] = AS_INSTANCE(object)->fields[index];
                NEXT();
            }
        }
        SYNC();
        vm->stack_top[-1] = get_field_slow(vm,object,name);
        NEXT();
    }
```

Un attribut d'objet est le cas rapide, avec la petite boucle de
`struct_field_index`. L'objet est remplacé par la valeur à sa place sur la
pile, sans `POP` ni `PUSH`. Tout le reste part dans `get_field_slow` : une
méthode lue sans parenthèses (section 13.7), une clé de map (`t.age`), un
élément d'énumération (13.8), et les erreurs. Le `SYNC()` n'est fait que sur
le chemin lent, le seul qui peut afficher une erreur (chapitre 11).

```c fichier="src/vm/vm.c" lignes="948-964" focus="956-957"
    CASE(OP_SET_FIELD):{
        ObjString *name = READ_STRING();
        Value value = POP();
        Value object = PEEK(0);
        SYNC();
        int index = IS_INSTANCE(object) ? struct_field_index(AS_INSTANCE(object)->structure,name) : -1;
        if(index >= 0){
            ObjInstance *instance = AS_INSTANCE(object);
            if(instance->structure->field_is_const[index] || instance->structure->field_types[index].kind != TYPE_ANY)
                check_field(vm,module,instance,index,value);
            instance->fields[index] = value;
        }else{
            set_field_slow(vm,object,name,value);
        }
        vm->stack_top[-1] = value;
        NEXT();
    }
```

Les lignes 956 et 957 sont la règle des types de `GRAMMAIRE.md` appliquée aux
attributs : un attribut sans type et sans `const` s'écrit directement. Les
autres passent par `check_field` :

```c fichier="src/vm/vm.c" lignes="337-350" fonction="check_field" focus="340-344"
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
```

Voici un détail du vrai code. Un attribut `const DIMENSION = 2` doit bien
recevoir sa valeur une fois. Qui a le droit de l'écrire ? La closure des
attributs, et elle seule. `check_field` regarde quelle closure tourne en ce
moment, `vm->frames[vm->frame_count - 1].closure`, et la compare à
`fields_init`. C'est aussi pour ça que `compile_fields_init` écrit
`SET_FIELD "v"` avec le nom, et pas l'instruction rapide qu'on verra en 13.5 :
il faut passer par cette vérification. Le chemin lent ne coûte qu'une fois par
objet.

`set_field_slow` (lignes 321 à 335) range une clé dans un map, refuse de
modifier une énumération, et donne les erreurs. Il refuse en particulier
d'écrire sur une méthode, avec un message à part : « `depose` n'est pas un
attribut ». On y reviendra en 13.7.

> [!essaie]
> Ajoute `const DIMENSION = 2` à `Point` dans `champ.cf`, et écris
> `p.DIMENSION = 3` à la fin. Le message arrive à l'exécution, après
> l'affichage. Écris ensuite `DIMENSION = 3` dans une méthode de `Point` :
> cette fois rien ne s'affiche, l'erreur arrive à la compilation. La section
> suivante dit pourquoi.

## 13.5 self sans l'écrire

Dans une méthode, la case 0 de la pile est `self`. Dans une fonction
ordinaire, c'est la fonction appelée elle-même (chapitre 10). C'est
`func_state_begin` qui décide, au tout début de chaque fonction compilée :

```c fichier="src/vm/compiler.c" lignes="270-273" fonction="func_state_begin"
    // La case 0 : self dans une méthode, la fonction appelée sinon.
    Local *slot = &fs->locals[fs->local_count++];
    *slot = (Local){.name = kind == FUNC_METHOD ? string_from(c->vm,"self") : NULL,.is_const = true};
    slot->type = (TypeRef){TYPE_ANY,NULL};
```

Dans une méthode, la case 0 porte le nom `self`. Ce n'est rien de plus qu'une
locale constante qu'on n'a pas eu besoin de déclarer. `return self` devient
`GET_LOCAL 0`, et on l'a vu dans `avance`. Quand le programmeur écrit `self`
dans ses paramètres, comme `func __init__(self, depart)` dans
`GRAMMAIRE.md`, le parser l'a déjà retiré de la liste (`src/parser.c`, ligne
486) : ce `__init__` a un seul paramètre, et `new Point(1)` le trouve.

C'est la différence avec Python, où `self` est un vrai premier paramètre, que
la méthode doit déclarer et que l'appel passe en douce. Dans Java et dans
C++, `this` est implicite comme dans CeFlo, et `x` seul désigne l'attribut.
CeFlo a pris les deux : `self` existe toujours, et on peut l'écrire si on
veut, mais on n'est pas obligé.

### Trouver un attribut par son nom

Pour `x` sans `self.`, il faut une étape dans la résolution des noms du
chapitre 10. Dans `resolve_in_function`, entre la recherche des locales et
celle des fonctions qui entourent, on ajoute la structure :

```c fichier="src/vm/compiler.c" lignes="443-455" fonction="resolve_in_function"
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
```

L'ordre reproduit la chaîne des portées du visitor : d'abord les locales (un
paramètre `x` cache l'attribut `x`), puis les attributs et les méthodes de la
structure, puis les fonctions qui entourent, puis les globales. Un nom trouvé
dans la structure devient un `NAME_FIELD` avec sa case, ou un `NAME_METHOD`.
Le modèle sert ici une deuxième fois : c'est lui qui donne la case de `x`
pendant la compilation de `avance`, bien avant qu'aucune structure n'existe à
l'exécution.

Le `Name` retient aussi où trouver `self` : `self_kind` et `self_index`, ici
la locale 0. Ces deux champs prennent leur sens dans le troisième bloc de la
fin de `resolve_in_function`, celui qu'on avait laissé de côté au chapitre
12 : si l'attribut est trouvé dans une fonction **qui entoure**, c'est `self`
qu'il faut capturer, pas l'attribut.

```ceflo fichier="liee.cf"
struct Compte {
    var solde = 0
    func depose(n) { solde += n }
    func plus_tard() {
        return lfunc(n) { depose(n) }
    }
}

var c = new Compte()
var f = c.depose
f(10)
println(f)
var g = c.plus_tard()
g(5)
println(c.solde)
c.depose = 3
```

```terminal
$ ./Ceflo liee.cf
<func depose>
15
erreur: `depose` n'est pas un attribut
  --> liee.cf:16:3
   16 | c.depose = 3
      |   ^
$ ./Ceflo --dis liee.cf
...
== plus_tard ==
0000     5  CLOSURE               0  <func lfunc>
0003      |                       local 0
0005     |  RETURN

== lfunc ==
0000     5  GET_UPVALUE           0
0002     |  GET_LOCAL             1
0004     |  INVOKE                0  "depose"  (1 args)
0008     |  POP
0009     |  NONE
0010     |  RETURN
...
```

La `lfunc` capture la case 0 de `plus_tard` (`local 0`), c'est-à-dire `self`.
`depose(n)` devient « pousse l'upvalue 0, pousse `n`, invoque `depose` ». Le
reste de `liee.cf` sert en 13.7.

### L'instruction rapide

Reste à écrire l'instruction. `emit_get_name`, du chapitre 10, gagne deux
cas :

```c fichier="src/vm/compiler.c" lignes="510-520" fonction="emit_get_name"
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
```

Quand `self` est la case 0 de la fonction qui tourne, un attribut se lit avec
`OP_GET_SELF_FIELD`, une instruction de deux octets dont l'opérande est la
case. Sinon, et en particulier quand `self` est une upvalue, on pousse `self`
et on retombe sur `OP_GET_FIELD` avec le nom. Le `< 256` vient de l'opérande
d'un octet. Dans `vm.c`, avec les instructions rapides :

```c fichier="src/vm/vm.c" lignes="1090-1091"
    CASE(OP_GET_SELF_FIELD): PUSH(AS_INSTANCE(slots[0])->fields[READ_BYTE()]); NEXT();
    CASE(OP_SET_SELF_FIELD): AS_INSTANCE(slots[0])->fields[READ_BYTE()] = POP(); NEXT();
```

Aucune vérification. Pas de test que `slots[0]` est bien un objet, pas de
recherche de nom, pas même de contrôle du type de l'attribut. Elles sont sûres
parce que le compilateur ne les écrit que dans une méthode, où la case 0 est
forcément un objet de cette structure. Pour l'écriture, c'est lui qui fait ce
que `OP_SET_FIELD` aurait vérifié :

```c fichier="src/vm/compiler.c" lignes="1149-1156" fonction="emit_self_field_store"
// self.x = v dans une méthode : la vérification que ferait OP_SET_FIELD est écrite ici.
static void emit_self_field_store(Compiler *c, int slot, ast_t *target){
    ObjStruct *model = c->current->structure->model;
    if(model->field_is_const[slot])
        visitor_error(target,"`%s` est une constante",model->field_names[slot]->chars);
    emit_check(c,CHECK_VARIABLE,model->field_types[slot],model->field_names[slot],target);
    emit_op_u8(c,OP_SET_SELF_FIELD,slot,target);
}
```

Une constante est refusée à la compilation : c'est la réponse de l'exercice
précédent. Un attribut typé reçoit un `OP_CHECK_TYPE` juste avant l'écriture,
le `CHECK_TYPE 0 variable x :: int` de `avance`. Un attribut sans type n'a
rien du tout. Ce que le compilateur vérifie une fois, la VM n'a plus à le
vérifier à chaque passage.

`self.x` écrit en entier mérite le même traitement. C'est le rôle de
`self_field_slot` (lignes 533 à 550) : pour `x` comme pour `self.x`, elle
retourne la case si `self` est la locale 0, et -1 sinon. Le compilateur
l'appelle pour lire (`compile_expr`, cas `AST_OBJ_ACCESS`) et pour écrire
(`compile_assignement`). `x += n` et `self.x += n` donnent le même bytecode.

### Ce que ça rapporte

Pour mesurer, j'ai compilé une copie de `src/` où `emit_get_name` et
`self_field_slot` n'utilisent jamais l'instruction rapide. La méthode
`avance` devient :

```terminal
$ ./Ceflo_sans_self --dis point.cf
...
== avance ==
0000     6  GET_LOCAL             0
0002     |  DUP
0003     |  GET_FIELD             0  "x"
0006     |  GET_LOCAL             1
0008     |  ADD
0009     |  SET_FIELD             0  "x"
0012     |  POP
0013     7  GET_LOCAL             0
0015     |  RETURN
...
```

Neuf instructions au lieu de sept, et surtout deux recherches par nom au lieu
de zéro. Sur `bench/methodes.cf`, qui appelle cinq millions de méthodes sans
`self.`, le meilleur temps sur douze essais est de 0,39 s avec la VM du dépôt
et de 0,52 s avec cette copie (la machine était chargée pendant la mesure :
les deux chiffres ont été pris au même moment).

## 13.6 new, __init__ et ses versions

`new Point()` devient deux instructions : `CONSTANT 4 "Point"`, puis
`NEW 4 "Point" (0 args)`. Le compilateur pousse le **nom** de la structure,
pas la structure :

```c fichier="src/vm/compiler.c" lignes="1112-1123" fonction="compile_new"
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
```

Une structure locale, comme la `Boite` de `usine.cf`, est poussée par sa case
(`GET_LOCAL 2`). Une globale est poussée par son nom, et `OP_NEW` la cherche
lui-même dans le module avec `find_struct` (`vm.c`, lignes 236 à 249). Ce
détour donne de meilleurs messages : si `Couleur` est une énumération, on lit
« `Couleur` est une énumération : ses valeurs existent déjà », et si `Point`
n'existe pas, « la structure `Point` n'est pas définie ». Un `GET_GLOBAL`
aurait donné l'erreur générale d'une globale inconnue.

```c fichier="src/vm/vm.c" lignes="988-997"
    CASE(OP_NEW):{
        ObjString *name = READ_STRING();
        int argc = READ_BYTE();
        SYNC();
        ObjStruct *structure = find_struct(vm,module,PEEK(argc),name);
        Value object = vm_new_instance(vm,structure,argc,vm->stack_top - argc);
        vm->stack_top -= argc + 1;
        PUSH(object);
        NEXT();
    }
```

Tout le travail est dans `vm_new_instance`, une fonction publique de `vm.h`
parce que `@Point(1, 2)` s'en sert aussi :

```c fichier="src/vm/vm.c" lignes="220-233" fonction="vm_new_instance"
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
```

Ligne par ligne :

- Choisir le bon `__init__`, c'est lire une case : `inits[argc]`. Pas de
  recherche, pas de comparaison de signatures.
- Sans argument et sans `__init__`, l'objet garde ses valeurs de départ. Avec
  des arguments et sans version qui les prend, c'est une erreur.
- `instance_new` alloue l'objet, toutes valeurs à `none`.
- `vm_push` le pose sur la pile. Les deux appels qui suivent passent par des
  points sûrs du GC, et l'objet n'est encore rangé nulle part ailleurs : sans
  ce `push`, le premier GC le libérerait pendant son propre `__init__`.
- `call_from_c` (chapitre 11) appelle une closure avec une valeur choisie
  dans la case 0. Ici, c'est l'objet : la closure des attributs et `__init__`
  s'exécutent avec le bon `self`. Elle relance une boucle `vm_run`, qui
  s'arrête au retour.

Ce que fait `call_from_c` sur la pile, pour `new Personne("Yann", 22)` :

```text
avant OP_NEW                 [ ...  "Personne"  "Yann"  22 ]
vm_push(instance)            [ ...  "Personne"  "Yann"  22  objet ]
call_from_c(fields_init)     [ ...  "Personne"  "Yann"  22  objet  objet ]
                                                             ^ case 0 de l'appel
call_from_c(init, 2 args)    [ ...  "Personne"  "Yann"  22  objet  objet "Yann" 22 ]
                                                             ^ self  ^ nom  ^ age
après OP_NEW                 [ ...  objet ]
```

Les arguments sont recopiés au-dessus pour l'appel, parce que `__init__` doit
avoir `self` juste avant eux. Copier deux ou trois valeurs de 16 octets ne
coûte presque rien, et laisse la pile de l'appelant intacte.

### Plusieurs __init__

Choisir une version selon le nombre d'arguments est la forme de surcharge la
plus simple. Java et C++ choisissent selon les types, ce qui demande de
connaître les types à la compilation. Python n'a qu'un seul `__init__`, et on
simule les autres avec des valeurs par défaut. Erlang et Elixir distinguent
leurs fonctions par leur nombre d'arguments, comme CeFlo le fait pour
`__init__`.

```ceflo fichier="personne.cf"
struct Personne {
    var Nom
    var Age::int

    func __init__() {
        Nom = "inconnu"
    }
    func __init__(nom, age) {
        Nom = nom
        Age = age
    }
    func __str__() {
        return Nom + " a " + Age + " ans"
    }
    func __cast__(texte) {
        var morceaux = texte.split(":")
        return new Personne(morceaux[0], @int(morceaux[1]))
    }
}

println(new Personne())
println(new Personne("Yann", 22))
println(@Personne("Grace:18"))
println(new Personne("Ali"))
```

```terminal
$ ./Ceflo personne.cf
inconnu a 0 ans
Yann a 22 ans
Grace a 18 ans
erreur: `Personne` n'a pas de __init__ qui prend 1 argument(s)
  --> personne.cf:24:9
   24 | println(new Personne("Ali"))
      |         ^
```

`Age` vaut 0 dans le premier objet : `var Age::int` sans valeur reçoit la
valeur par défaut de son type (`emit_default`, chapitre 10), calculée par la
closure des attributs avant `__init__`.

### __str__ et __cast__

Les deux autres méthodes spéciales sont appelées depuis le C. `__str__` sert
dès qu'un objet devient du texte, dans `src/vm/print.c` :

```c fichier="src/vm/print.c" lignes="95-113" fonction="print_instance"
static void print_instance(VM *vm, buffer_t *buffer, Value value){
    ObjInstance *instance = AS_INSTANCE(value);

    if(IS_NONE(instance->structure->str_method)){
        Buffer_Append_Char(buffer,'<');
        Buffer_Append_Str(buffer,instance->structure->name->chars);
        Buffer_Append_Char(buffer,'>');
        return;
    }

    Value method = OBJ_VAL(bound_method_new(vm,value,instance->structure->str_method));
    vm_push(vm,method);
    Value result = vm_call(vm,method,0,NULL);
    vm_pop(vm);

    vm_push(vm,result);
    print_value(vm,buffer,result);
    vm_pop(vm);
}
```

Sans `__str__`, l'objet s'écrit `<Personne>`. Avec, `print_instance` fabrique
une méthode liée (on la verra en 13.7) et l'appelle avec `vm_call`. Le
résultat est poussé sur la pile avant d'être affiché : `print_value` peut
appeler un autre `__str__`, si `__str__` retourne une liste d'objets par
exemple, et le résultat doit rester vivant pendant ce temps.

`__cast__` sert à `@Personne(x)` avec un seul argument, dans
`src/vm/natives/typer.c` :

```c fichier="src/vm/natives/typer.c" lignes="236-241"
    // @Point(1, 2) : une structure se construit comme avec `new`, sauf si elle a un __cast__
    if(kind == TYPE_NAMED){
        Value structure = typer_find_type(vm,module,type_name);
        if(IS_STRUCT(structure) && (argc != 1 || IS_NONE(AS_STRUCT(structure)->cast_method)))
            return vm_new_instance(vm,AS_STRUCT(structure),argc,args);
    }
```

Avec plusieurs arguments, ou sans `__cast__`, `@Personne(...)` est un `new`.
Avec un seul argument et un `__cast__`, `typer_to_struct` (lignes 159 à 177)
rend tel quel un objet déjà du bon type ou `none`, et sinon appelle
`__cast__` avec `vm_call`. Le résultat doit être un objet de la structure,
sinon `@Personne(x)` donne « impossible de convertir ».

> [!essaie]
> Donne à `Personne` un troisième `__init__`, à un seul paramètre, qui
> découpe `"Ali:30"`. `new Personne("Ali:30")` marche alors. Mais
> `@Personne("Ali:30")` appelle-t-il ton `__init__` ou `__cast__` ? Relis la
> condition de la ligne 239 de `typer.c` avant de lancer le programme.

## 13.7 Appeler une méthode

On a tout ce qu'il faut pour appeler une méthode, sans aucune instruction de
plus. `p.avance(2)` est un appel dont l'appelé est `p.avance` : on lit
`p.avance` avec `OP_GET_FIELD`, puis on l'appelle avec `OP_CALL`. Il faut
seulement que `p.avance` donne une valeur qui se souvient de `p`. Sinon
`avance` n'aurait pas de `self`.

Cette valeur est la **méthode liée**, `ObjBoundMethod` : une méthode et son
objet. Elle est déjà dans `object.h` depuis le chapitre 11 (lignes 231 à 237),
avec deux champs, `receiver` et `method`. On la crée dans `get_field_slow`,
quand le nom n'est pas un attribut :

```c fichier="src/vm/vm.c" lignes="289-296"
// o.nom quand o n'est pas un objet dont `nom` est un attribut.
static Value get_field_slow(VM *vm, Value object, ObjString *name){
    if(IS_INSTANCE(object)){
        Value method;
        if(table_get(&AS_INSTANCE(object)->structure->methods,name,&method))
            return OBJ_VAL(bound_method_new(vm,object,method));
        vm_error(vm,"`%s` n'a pas d'attribut `%s`",value_type_name(object),name->chars);
    }
```

Et quand on l'appelle, `call_value` remet l'objet dans la case 0, à la place
de la méthode liée, puis appelle la vraie méthode :

```c fichier="src/vm/vm.c" lignes="182-186"
            case OBJ_BOUND_METHOD:{
                ObjBoundMethod *bound = AS_BOUND_METHOD(callee);
                vm->stack_top[-argc - 1] = bound->receiver;
                return call_value(vm,bound->method,argc);
            }
```

```text
GET_GLOBAL p           [ p ]
GET_FIELD "avance"     [ <lié : p, avance> ]          un objet alloué
CONSTANT 2             [ <lié : p, avance>  2 ]
CALL 1                 [ p  2 ]                       la case 0 redevient p
                         ^ self de avance
```

C'est la version simple, et elle marche. Pour la voir tourner, j'ai compilé une
copie de `src/` où `compile_call` n'a pas sa première branche (on va l'écrire
juste après). `point.cf` donne le bon résultat, avec ce bytecode :

```terminal
$ ./Ceflo_sans_invoke point.cf
5 0
$ ./Ceflo_sans_invoke --dis point.cf
...
0026    12  GET_GLOBAL            1
0029     |  GET_FIELD             2  "avance"
0032     |  CONSTANT              5  2
0035     |  CALL                  1
0037     |  GET_FIELD             2  "avance"
0040     |  CONSTANT              6  3
...
```

Elle a deux défauts. Le premier se voit tout de suite :

```terminal
$ ./Ceflo_sans_invoke liste.cf
erreur: list n'a pas d'attribut `append`
  --> liste.cf:2:3
    2 | l.append(2)
      |   ^
```

`liste.cf` est `var l = [1]` suivi de `l.append(2)`. Les méthodes des listes
sont écrites en C (chapitre 14) et ne sont pas des valeurs : il n'y a rien à
lier. Il faudrait fabriquer une méthode liée native, juste pour l'appeler.
Le second défaut est le coût. Chaque appel alloue un objet de 48 octets
(l'en-tête et deux `Value`) qu'on jette une instruction plus tard. Une boucle
qui appelle trois millions de méthodes en alloue trois millions, et le GC doit
toutes les ramasser.

### OP_INVOKE

D'où l'instruction `OP_INVOKE`, qui lit et appelle d'un coup. Dans `chunk.h`,
elle a deux opérandes, le nom et le nombre d'arguments :

```c fichier="src/vm/chunk.h" lignes="80-80"
        OP_INVOKE,          // u16 nom, u8 n   [o a1..an] -> [résultat]     o.nom(a1..an)
```

`compile_call`, au chapitre 10, compilait l'appelé puis les arguments. On
ajoute en tête un cas pour un appelé écrit `o.nom` :

```c fichier="src/vm/compiler.c" lignes="1076-1081" fonction="compile_call"
    if(callee->type == AST_OBJ_ACCESS){
        compile_expr(c,callee->left);
        compile_args(c,node);
        emit_invoke(c,node,string_from(c->vm,callee->variable_name),callee);
        return;
    }
```

On compile `o`, pas `o.nom`, puis les arguments. La pile contient alors
l'objet et ses arguments : exactement la forme d'un appel de méthode, l'objet
déjà dans ce qui deviendra la case 0. `emit_invoke` (lignes 991 à 996) écrit
l'instruction, avec le noeud de `o.nom` pour le nom : une erreur « n'a pas de
méthode » pointera sur le nom, pas sur la parenthèse.

Côté VM, `OP_INVOKE` lit ses opérandes et appelle `invoke`, qui doit trouver
la méthode et dire si un `CallFrame` a été ouvert :

```c fichier="src/vm/vm.c" lignes="865-872"
    CASE(OP_INVOKE):{
        ObjString *name = READ_STRING();
        int argc = READ_BYTE();
        SYNC();
        ast_t *name_node = frame->closure->function->chunk.nodes[ip - 3 - frame->closure->function->chunk.code];
        CALLED(invoke(vm,name,argc,name_node));
        NEXT();
    }
```

La ligne 869 retrouve le noeud du nom : `ip - 3` pointe sur le premier octet
du nom (le `u16` puis le `u8` viennent d'être lus), et `emit_invoke` a donné
à ces deux octets le noeud de `o.nom`.

```c fichier="src/vm/vm.c" lignes="358-372" fonction="invoke"
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
```

Un attribut passe d'abord : si `o.f` contient une `lfunc`, `o.f()` l'appelle,
et la `lfunc` prend la case 0 à la place de l'objet (elle n'est pas une
méthode, et n'a pas de `self`). Sinon, la méthode est cherchée dans la table
de la structure, et appelée avec l'objet là où il est. Aucune allocation. La
suite de `invoke` (lignes 374 à 389) traite une fonction rangée dans un map,
puis les méthodes écrites en C des listes, des maps, des chaînes et des
énumérations, avec `natives_call_method` : c'est le chapitre 14.

CPython a fait exactement le même chemin. Jusqu'à Python 3.6, `p.avance(2)`
créait un objet « bound method » à chaque appel. Python 3.7 a ajouté deux
instructions, `LOAD_METHOD` et `CALL_METHOD`, pour l'éviter, et le gain sur
les appels de méthodes était d'environ 20 %. *Crafting Interpreters* consacre
une section entière à la même optimisation, qu'il appelle aussi `OP_INVOKE`.

### La mesure

Les deux programmes suivants font la même chose, trois millions de dépôts.
`invoke.cf` écrit `c.depose(1)` ; `liee_boucle.cf` écrit `var f = c.depose`
puis `f(1)`, ce qui force la méthode liée même avec la VM du dépôt :

```ceflo fichier="invoke.cf"
struct Compte {
    var solde = 0
    func depose(n) { solde += n }
}
var c = new Compte()
for var i = 0; i < 3000000; i += 1 {
    c.depose(1)
}
println(c.solde)
```

Meilleur temps sur dix essais, pris au même moment sur une machine chargée :
0,24 s pour `invoke.cf` avec la VM du dépôt, 0,37 s pour `invoke.cf` avec la
copie sans `OP_INVOKE`, 0,40 s pour `liee_boucle.cf`. La méthode liée coûte
plus de la moitié du temps de l'appel.

### Une méthode n'est pas un attribut

Il reste le message de la fin de `liee.cf`. `c.depose = 3` est refusé, alors
que dans le visitor on pouvait écrire par-dessus une méthode. C'est une
différence voulue de la VM (`docs/VM.md`) : un attribut garde sa place, et
`depose` n'en a pas. Si on acceptait cette écriture, il faudrait donner à
chaque objet une case de plus par méthode, ou une table de secours, et
`invoke` devrait la consulter à chaque appel avant les méthodes. Pour changer
le comportement d'un objet, on range une `lfunc` dans un attribut : `invoke`
la trouve en premier.

> [!essaie]
> Dans `invoke.cf`, remplace `c.depose(1)` par `depose_de(c)`, avec une
> fonction globale `func depose_de(o) { o.solde += 1 }`. Désassemble les deux
> versions et compte les instructions exécutées à chaque tour. Puis mesure
> avec `/usr/bin/time -p`. La fonction globale n'a pas `OP_GET_SELF_FIELD` :
> est-ce que ça se voit ?

## 13.8 Les énumérations et match

Une énumération est la petite soeur d'une structure. Elle a un nom, des
éléments fixés à la compilation, et chaque élément n'existe qu'une fois. On
ajoute dans `object.h`, sous les structures :

```c fichier="src/vm/object.h" lignes="277-291"
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
```

« Comparés par leur adresse » est la règle de `GRAMMAIRE.md` :
`Couleur.ROUGE == 0` vaut `false`. `OP_EQUAL` n'a rien à faire de
particulier, puisque `value_equals` compare deux objets par leur pointeur.
`owner` permet de savoir à quelle énumération appartient un élément, pour
vérifier `var c::Couleur` (`typer_check`, ligne 74 de `typer.c`).

En C, une énumération est un entier déguisé : `ROUGE == 0` est vrai, et on
peut passer `42` là où une `Couleur` est attendue. En Java, chaque élément est
un objet unique, comparé par adresse, comme dans CeFlo. Rust va plus loin :
un élément peut porter des données, et `match` vérifie à la compilation que
tous les cas sont traités. CeFlo reste à la hauteur de Java, et fait la
vérification de `match` à l'exécution.

Comme pour une structure, le compilateur construit un modèle avec tous ses
éléments (`compile_enum_definition`, lignes 849 à 865), et `OP_ENUM` en fait
une copie à chaque exécution de la définition avec `new_enum` (`vm.c`, lignes
262 à 268). Une énumération définie dans une fonction a donc des éléments
neufs à chaque appel : `tests/vm/struct_locale.cf` vérifie que
`couleur() == couleur()` vaut `false`. La fin de `compile_enum_definition` a
un détail :

```c fichier="src/vm/compiler.c" lignes="863-865"
    emit_op_u16(c,OP_ENUM,make_constant(c,OBJ_VAL(model),node),node);
    define_top(c,name,DEF_ENUM,true,node);
}
```

Le `true` en fait une constante. `Couleur = 1` est refusé à la compilation,
avec le message d'un `const` :

```terminal
$ ./Ceflo enumconst.cf
erreur: `Couleur` est une constante
  --> enumconst.cf:3:1
    3 | Couleur = 1
      | ^
```

(`enumconst.cf` commence par un `println("debut")`, qui ne s'affiche pas :
l'erreur est bien trouvée avant l'exécution.)

`Couleur.ROUGE` se lit avec `OP_GET_FIELD`, qui part dans `get_field_slow`
puisque `Couleur` n'est pas un objet :

```c fichier="src/vm/vm.c" lignes="303-316"
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
```

`enum_find` parcourt les éléments en comparant les adresses des noms, comme
`struct_field_index`. `.name` et `.value` sont deux attributs câblés en dur,
avec un `strcmp`, ce qui est plus lent qu'une comparaison de pointeurs : on
pourrait interner ces deux noms dans la VM, comme `name_init`.

### match

C'est avec `match` que les énumérations prennent leur sens. Voici un `match`
sans `else`, dans une fonction qui reçoit une `Couleur` :

```ceflo fichier="couleur.cf"
enum Couleur { ROUGE, VERT, BLEU }

func decris(c::Couleur) {
    match c {
        case Couleur.ROUGE { return "chaud" }
        case Couleur.VERT, Couleur.BLEU { return "froid" }
    }
}

println(decris(Couleur.VERT), " ", Couleur.BLEU.value, " ", @Couleur(0))
println(decris(none))
```

```terminal
$ ./Ceflo couleur.cf
froid 2 Couleur.ROUGE
erreur: ce match ne traite pas none
  --> couleur.cf:4:5
    4 |     match c {
      |     ^
```

`none` passe la vérification de type (une variable de type énumération
accepte `none`), mais aucun `case` ne le traite, et `GRAMMAIRE.md` dit que
c'est une erreur dès qu'une énumération est en jeu. Le bytecode :

```terminal
$ ./Ceflo --dis couleur.cf
...
== decris ==
0000     3  GET_LOCAL             1
0002     |  CHECK_TYPE            0  argument `c` :: Couleur
0005     |  POP
0006     4  GET_LOCAL             1
0008     5  GET_LOCAL             2
0010     |  GET_GLOBAL            0
0013     |  GET_FIELD             0  "ROUGE"
0016     |  EQUAL
0017     |  JUMP_IF_FALSE         7  -> 0027
0020     |  CONSTANT              1  "chaud"
0023     |  RETURN
0024     |  JUMP                 35  -> 0062
0027     6  GET_LOCAL             2
0029     |  GET_GLOBAL            0
0032     |  GET_FIELD             2  "VERT"
0035     |  NOT_EQUAL
0036     |  JUMP_IF_FALSE        12  -> 0051
0039     |  GET_LOCAL             2
0041     |  GET_GLOBAL            0
0044     |  GET_FIELD             3  "BLEU"
0047     |  EQUAL
0048     |  JUMP_IF_FALSE         7  -> 0058
0051     |  CONSTANT              4  "froid"
0054     |  RETURN
0055     |  JUMP                  4  -> 0062
0058     4  GET_LOCAL             2
0060     |  MATCH_FAIL            1
0062     |  POP
0063     3  NONE
0064     |  RETURN
...
```

À l'octet 6, `GET_LOCAL 1` copie le sujet, qui devient la case cachée 2 : il
ne sera calculé qu'une fois, comme le veut `GRAMMAIRE.md`. Chaque `case`
compare cette case à sa valeur. Le deuxième `case` a deux valeurs, et
`compile_match` les enchaîne d'une façon qui surprend à la première lecture :

```c fichier="src/vm/compiler.c" lignes="1344-1357" fonction="compile_match"
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
```

Pour toutes les valeurs sauf la dernière, `NOT_EQUAL` puis `JUMP_IF_FALSE`
saute **dans** le corps si elles sont égales (octet 36 vers 51) : « pas
différent » veut dire « égal ». Seule la dernière saute au `case` suivant si
elle est différente. On n'a qu'un seul type de saut conditionnel, et on évite
ainsi un `OP_NOT` par valeur.

Après chaque corps, un `JUMP` saute à la fin. Ici il ne sert jamais, puisque
les corps finissent par `RETURN`, mais le compilateur ne cherche pas à le
savoir : deux octets morts valent mieux qu'une analyse de plus. Si rien ne
correspond, on arrive à l'octet 58 : le sujet est poussé une dernière fois,
et `MATCH_FAIL 1` décide :

```c fichier="src/vm/vm.c" lignes="1024-1033"
    CASE(OP_MATCH_FAIL):{
        int has_enum = READ_BYTE();
        Value subject = PEEK(0);
        if(has_enum || IS_ENUM_VALUE(subject)){
            SYNC();
            vm_error(vm,"ce match ne traite pas %s",vm_to_string(vm,subject)->chars);
        }
        vm->stack_top--;
        NEXT();
    }
```

L'opérande `1` vient de la ligne 1346 : `compile_match` a vu une valeur écrite
`a.b`. Il ne peut pas savoir à la compilation que `Couleur.ROUGE` est un
élément d'énumération et pas un attribut d'objet. C'est la limite écrite dans
`docs/VM.md` : un `case p.x` qui ne correspond pas donne cette erreur, même
sans énumération. La VM complète le test à l'exécution avec
`IS_ENUM_VALUE(subject)`, pour un sujet énumération dont les `case` seraient
des variables.

Tu peux maintenant écrire en CeFlo tout ce que le `README` montre : des
structures avec leurs attributs typés, plusieurs `__init__`, `__str__` et
`__cast__`, des méthodes qui s'appellent sans `self.`, des énumérations qu'on
passe à `match`. Chaque attribut lu dans une méthode coûte une instruction de
deux octets.

Un programme réel tient rarement dans un seul fichier, pourtant. Il lit des
fichiers avec `os`, découpe du texte avec `split`, importe ses propres
modules, et parfois une bibliothèque écrite en C. Ces fonctions qu'on appelle
sans les avoir écrites en CeFlo, `invoke` les cherchait déjà dans sa dernière
ligne. Le chapitre 14 ouvre cette porte, et c'est le dernier.

## Défis

1. **`__eq__`.** Fais que `a == b`, entre deux objets d'une structure qui a
   une méthode `__eq__(autre)`, appelle cette méthode au lieu de comparer les
   adresses. Range-la à part, comme `str_method` (`object.h`, `define_method`
   dans `vm.c`), et traite le cas dans `OP_EQUAL` et `OP_NOT_EQUAL`. Attention :
   `OP_EQUAL` ne fait pas de `SYNC()` aujourd'hui, et un appel de CeFlo en
   demande un. Pense aussi au GC dans `memory.c`.

2. **Plus de sept paramètres.** Le tableau `inits[8]` limite `__init__` à 7
   paramètres. Remplace-le par une petite liste triée par nombre de
   paramètres, ou par une `Table` dont la clé serait le nombre écrit en texte.
   Mesure avec `bench/objets.cf` que `new` ne ralentit pas.

3. **Un cache sur `OP_GET_FIELD`.** Hors d'une méthode, `p.x` appelle
   `struct_field_index` à chaque fois. Ajoute au chunk un tableau parallèle au
   code, qui garde pour chaque `OP_GET_FIELD` la dernière structure vue et la
   case trouvée. Si la structure de l'objet est la même, on lit la case sans
   chercher. C'est l'**inline cache** des moteurs JavaScript. Mesure le gain
   sur une boucle qui lit `p.x` depuis le haut du fichier.

4. **`match` exhaustif à la compilation.** Quand le sujet d'un `match` est
   une variable déclarée avec un type énumération (`c::Couleur`) et que
   l'énumération est une globale déjà compilée, le compilateur peut savoir
   quels éléments manquent. Fais-lui afficher un avertissement (sans arrêter
   le programme) qui les nomme. Regarde `compile_match` et `module->types`.

## Note de conception

Ce chapitre a retiré à CeFlo un peu de liberté. Dans le visitor, un objet
était une portée, et une portée accepte tout : on pouvait écrire par-dessus
une méthode, et une méthode ne savait pas à l'avance quels noms seraient des
attributs. Dans la VM, la forme d'un objet est décidée par sa définition, et
le compilateur s'en sert partout : pour la case de `x`, pour refuser une
constante, pour placer `CHECK_TYPE` au bon endroit.

On entend souvent qu'un langage de script doit être dynamique, et que figer
la forme des objets, c'est faire du Java. Je pense le contraire. Les
programmes réels changent très rarement la forme de leurs objets après les
avoir créés. Les moteurs JavaScript l'ont mesuré, et c'est sur cette régularité
qu'ils ont bâti leurs classes cachées : ils dépensent une énorme complexité
pour retrouver, pendant l'exécution, une information que le programmeur
connaissait en écrivant son code. Python a ajouté `__slots__`, puis des
dictionnaires partagés entre objets, pour la même raison.

CeFlo demande cette information au programmeur, qui la donne en écrivant
`var x` dans sa structure. Il l'écrivait déjà. En échange, il reçoit des
erreurs plus tôt (« `DIMENSION` est une constante » avant même que le
programme démarre), et des méthodes trois à quatre fois plus rapides que dans
le visitor (`make bench`, chapitre 14), avec une VM dont toute la partie objets tient en quelques centaines
de lignes. Pour le cas vraiment dynamique, des clés qu'on ne connaît pas à
l'avance, il y a le `map`, et `t.age` marche aussi.

La vraie limite est ailleurs : CeFlo ne sait pas, hors d'une méthode, ce
qu'est `p`. Sans types obligatoires, le compilateur ne peut pas remplacer
`p.x` par une case, et c'est là que le cache du défi 3 prend le relais.
Ajouter des types partout aurait donné un autre langage. On a pris le gain là
où il était gratuit, dans les méthodes, où `self` a toujours le même type.
