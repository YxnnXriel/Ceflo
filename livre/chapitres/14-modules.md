---
numero: 14
titre: modules et natives
note: ouvrir la porte au C
partie: Le bytecode
---

## 14.1 Un programme en plusieurs fichiers

Tous les programmes de ce livre tenaient dans un fichier. Un vrai programme,
non. Il a ses outils dans un fichier, ses structures dans un dossier, et il
lit le disque avec des fonctions qu'il n'a pas écrites. Voici trois fichiers,
rangés dans `livre/essais/chapitres-12-14/modules/` :

```ceflo fichier="outils.cf"
println("outils.cf s'exécute")

const VERSION = "1.0"
var appels = 0

func double(x) {
    appels += 1
    return x * 2
}

func secret() { return 42 }

export :: [VERSION, double, appels]
```

```ceflo fichier="geo/point.cf"
struct Point {
    var x = 0
    var y = 0
    func __init__(a, b) { x = a; y = b }
    func __str__() { return "(" + x + ", " + y + ")" }
}
```

```ceflo fichier="principal.cf"
from outils import [double, VERSION]
from outils import [appels]
from geo.point import *

println(double(21), " ", VERSION)
println(new Point(1, 2))
println(appels)
```

```terminal
$ ./Ceflo principal.cf
outils.cf s'exécute
42 1.0
(1, 2)
0
```

Trois choses à remarquer avant d'ouvrir le code. `outils.cf` est importé deux
fois, mais il ne s'exécute qu'une fois : la ligne « s'exécute » n'apparaît
qu'une fois. `from geo.point` a trouvé le fichier `geo/point.cf`. Et la
dernière ligne affiche `0`, alors que `double` vient d'augmenter `appels` :
on y reviendra, c'est le détail le plus instructif du chapitre.

Le bytecode du fichier principal tient en quelques instructions qu'on n'a pas
encore écrites :

```terminal
$ ./Ceflo --dis principal.cf
== principal.cf ==
0000     1  IMPORT                0  "outils.cf"
0003     |  IMPORT_NAME           1  "double"
0006     |  IMPORT_NAME           2  "VERSION"
0009     |  POP
0010     2  IMPORT                0  "outils.cf"
0013     |  IMPORT_NAME           3  "appels"
0016     |  POP
0017     3  IMPORT                4  "geo/point.cf"
0020     |  IMPORT_ALL
0021     5  GET_GLOBAL            3
0024     |  GET_GLOBAL            0
...
```

Le parser a déjà fait sa part (chapitre 3) : `outils` est devenu le texte
`"outils.cf"`, et `geo.point` est devenu `"geo/point.cf"`
(`parser_parse_include_path`, ligne 879 de `src/parser.c`). Le compilateur et
la VM reçoivent un chemin. Ce chapitre construit ce qui manque :

1. charger un fichier une seule fois, et en faire un module (14.2) ;
2. copier ses noms dans le fichier qui l'importe, avec `export` (14.3) ;
3. les builtins, ces fonctions C visibles partout (14.4) ;
4. les méthodes C des listes, des maps et des chaînes (14.5) ;
5. les modules `os` et `console`, écrits en C mais importés comme un fichier
   (14.6) ;
6. une bibliothèque `.so` qu'on écrit soi-même et qu'on charge pendant
   l'exécution (14.7).

Puis on fera les comptes : ce qu'on a construit en quatorze chapitres, ce que
ça vaut face à Python, et comment écrire ton propre langage (14.8 et 14.9).

## 14.2 Un fichier, un module, une seule fois

Un module est un `ObjModule`. On l'a construit au chapitre 10 pour ranger les
globales d'un fichier : un tableau de valeurs, le nom de chaque case, son type,
sa constance, et une table du nom vers le numéro. Deux champs restaient
inutilisés, `exports` et `loaded`. Ils servent maintenant.

```c fichier="src/vm/object.h" lignes="304-316"
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
```

Le fichier lancé depuis la ligne de commande est déjà un module : `vm_run_file`
appelle `run_source`, dans `src/vm/module.c`. Un fichier importé passera par
la même fonction :

```c fichier="src/vm/module.c" lignes="36-51" fonction="run_source" focus="40-41"
// Analyse, compile et exécute un fichier. Le module est rangé avant l'exécution.
static ObjModule *run_source(VM *vm, lexer_t *lexer){
    ast_t *program = parser_parse(lexer);

    ObjModule *module = module_new(vm,string_from(vm,lexer->fileName));
    table_set(&vm->modules,module->path,OBJ_VAL(module));

    ObjFunction *function = compile_module(vm,program,module);
    if(vm->disassemble)
        disassemble_function(function);

    Value closure = OBJ_VAL(closure_new(vm,function));
    vm_call(vm,closure,0,NULL);
    module->loaded = true;
    return module;
}
```

C'est toute la chaîne du livre en six lignes : le parser, le compilateur, la
closure du code du fichier, et `vm_call` qui l'exécute dans une boucle
`vm_run` à elle. Un import est un appel de fonction comme un autre : la VM est
réentrante (chapitre 11), et le fichier importé tourne au-dessus de celui qui
l'importe, sur la même pile.

`vm->modules` est une `Table` du chemin vers le module. C'est elle qui garantit
qu'un fichier ne s'exécute qu'une fois. Regarde où est le `table_set` : **avant**
`compile_module` et `vm_call`. On aurait pu le mettre à la fin, une fois le
fichier fini. Ce serait une erreur, à cause de ce cas :

```ceflo fichier="a.cf"
println("a commence")
from b import [nom_b]
var nom_a = "A"
println("a lit ", nom_b)
```

```ceflo fichier="b.cf"
println("b commence")
from a import *
var nom_b = "B"
println("b voit nom_a ? ", nom_a)
```

```terminal
$ ./Ceflo a.cf
a commence
b commence
erreur: `nom_a` n'est pas définie
  --> b.cf:4:28
    4 | println("b voit nom_a ? ", nom_a)
      |                            ^
```

`a` importe `b`, qui importe `a`. Si `a` n'était rangé qu'à la fin, `b`
chargerait `a` une deuxième fois, qui chargerait `b`, et ainsi de suite
jusqu'à « trop d'appels imbriqués ». Rangé dès le début, `a` est trouvé dans
la table par `b`, à moitié exécuté : sa première ligne est faite, `nom_a`
n'existe pas encore. `b` reçoit donc ce qui est défini, c'est-à-dire rien, et
l'erreur est claire. L'ancien interpréteur donne exactement la même sortie.

Python se comporte de la même façon : un module est rangé dans `sys.modules`
avant d'être exécuté, et un import circulaire voit un module « partiellement
initialisé ». Node.js aussi, avec ses `require` qui retournent un objet
incomplet. C'est le prix d'une règle simple, et les trois langages l'ont
choisie.

### Trouver le fichier

`OP_IMPORT` appelle `module_import` avec le chemin écrit dans le code :

```c fichier="src/vm/module.c" lignes="76-95" fonction="module_import"
Value module_import(VM *vm, ObjString *path){
    ObjString *full = resolve_path(vm,path);
    Value loaded;
    ObjModule *module = NULL;

    if(table_get(&vm->modules,full,&loaded)){
        module = AS_MODULE(loaded);
    }else{
        lexer_t lexer;
        if(newLexer(&lexer,full->chars))
            module = run_source(vm,&lexer);
        else
            module = natives_standard_module(vm,path->chars);
        if(module == NULL)
            vm_error(vm,"impossible d'ouvrir `%s`",path->chars);
    }

    last_import_path = path;
    return OBJ_VAL(module);
}
```

L'ordre des essais est la règle de `GRAMMAIRE.md` : un fichier trouvé passe
avant un module fourni par CeFlo. `newLexer` ouvre le fichier (chapitre 1) et
retourne `false` s'il n'existe pas ; alors seulement on demande à
`natives_standard_module` si `os.cf` ou `console.cf` est un module écrit en C
(section 14.6). Un fichier `os.cf` à côté du script cache donc le module `os`.

`resolve_path` (lignes 53 à 74) essaie d'abord le chemin à côté du fichier qui
importe. Elle le trouve grâce au noeud de l'instruction en cours,
`vm_current_node(vm)`, dont le champ `file` est le nom du fichier : c'est la
même information qui sert à afficher les erreurs depuis le chapitre 1. Si le
fichier n'est pas là, le chemin est gardé tel quel, et `newLexer` le cherchera
depuis le dossier courant.

Un détail : la clé de `vm->modules` est le chemin **résolu**. Deux fichiers de
dossiers différents qui écrivent `from outils import *` désignent deux
`outils.cf` différents, et chacun aura son module. Mais un même fichier atteint
par deux chemins écrits différemment (`outils.cf` et `./outils.cf`) serait
chargé deux fois. Python règle ça en rangeant ses modules par nom qualifié,
`geo.point`, plutôt que par chemin.

> [!essaie]
> Lance `./Ceflo modules/principal.cf` depuis `livre/essais/chapitres-12-14/`, au lieu de
> te placer dans `modules/`. Tout marche encore : le chemin est cherché à côté
> du fichier qui importe. Puis écris dans `principal.cf` un import de
> `"./outils.cf"` en plus de celui de `outils`. Combien de fois `outils.cf`
> s'exécute-t-il ?

## 14.3 Copier les noms

`OP_IMPORT` pose le module sur la pile. Il faut maintenant que `double`, dans
`principal.cf`, désigne la fonction de `outils.cf`. Le compilateur ne connaît
pas `outils.cf` : il ne l'a pas encore lu, et il ne le lira peut-être jamais
s'il est déjà chargé. Il fait donc de `double` une globale de `principal.cf`,
comme n'importe quel nom inconnu, et c'est la VM qui la remplira au moment de
l'import. On ajoute à `compiler.c`, dans la section `$MODULES` :

```c fichier="src/vm/compiler.c" lignes="1518-1546" fonction="compile_include" focus="1539-1545"
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
```

Trois formes, trois façons de faire. Commençons par la plus fréquente, en bas :
un import par noms au niveau du fichier. Pour chaque nom, `global_index` crée
la case dans le module courant (la case 0 pour `double`, la case 1 pour
`VERSION`), et `OP_IMPORT_NAME` la remplira. Le module reste sur la pile
pendant tous les noms, et un `POP` final le retire. C'est le `IMPORT`,
`IMPORT_NAME`, `IMPORT_NAME`, `POP` du désassemblage :

```text
                          pile                globales de principal.cf
IMPORT "outils.cf"        [ outils ]          double = ?   VERSION = ?
IMPORT_NAME "double"      [ outils ]          double = <func double>
IMPORT_NAME "VERSION"     [ outils ]          VERSION = "1.0" (constante)
POP                       [ ]
```

`add_file_name` a un rôle qu'on reverra en 14.4 : un nom importé cache un
builtin du même nom.

Côté VM, `OP_IMPORT_NAME` appelle `module_import_name`, qui cherche le nom puis
le copie :

```c fichier="src/vm/module.c" lignes="107-128"
// La globale `from->globals[index]` devient aussi une globale de `into`, avec son type et sa constance.
static void copy_global(VM *vm, ObjModule *into, ObjModule *from, int index){
    int target = module_global_index(vm,into,from->names[index]);
    into->globals.values[target] = from->globals.values[index];
    into->types[target] = from->types[index];
    into->is_const[target] = from->is_const[index];
}

// La case de `name` dans `from`, après avoir vérifié qu'il existe et qu'il est exporté.
static int find_export(VM *vm, ObjModule *from, ObjString *name){
    const char *path = last_import_path != NULL ? last_import_path->chars : from->path->chars;
    Value index;
    if(!table_get(&from->indexes,name,&index) || IS_UNDEFINED(from->globals.values[(int)AS_NUMBER(index)]))
        vm_error(vm,"`%s` n'existe pas dans `%s`",name->chars,path);
    if(!is_exported(from,name))
        vm_error(vm,"`%s` n'est pas exporté par `%s`",name->chars,path);
    return (int)AS_NUMBER(index);
}

void module_import_name(VM *vm, ObjModule *into, ObjModule *from, ObjString *name){
    copy_global(vm,into,from,find_export(vm,from,name));
}
```

`copy_global` copie trois choses : la valeur, le type et la constance.
`VERSION` reste donc une constante dans `principal.cf`, et `VERSION = "2.0"`
y serait refusé. Un `var age::int` importé garde son type. Les deux erreurs
de `find_export` se voient ainsi :

```terminal
$ ./Ceflo secret.cf
outils.cf s'exécute
erreur: `secret` n'est pas exporté par `outils.cf`
  --> secret.cf:1:21
    1 | from outils import [secret]
      |                     ^
$ ./Ceflo absent.cf
outils.cf s'exécute
erreur: `absent` n'existe pas dans `outils.cf`
  --> absent.cf:1:21
    1 | from outils import [absent]
      |                     ^
```

La flèche pointe sur le nom, pas sur `from` : le compilateur a donné à chaque
`OP_IMPORT_NAME` le noeud de son nom. `last_import_path` garde le chemin tel
qu'il est écrit, pour que le message dise `outils.cf` et pas un chemin résolu
qui commencerait par un dossier.

### Une copie, pas un lien

On peut maintenant expliquer le `0` de la section 14.1. `from outils import
[appels]` a copié la **valeur** de `appels` au moment de l'import, quand elle
valait 0. Ensuite, `double(21)` exécute le code de `outils.cf`, qui lit et
écrit la globale `appels` de **son** module, avec `GET_GLOBAL` et
`SET_GLOBAL` sur ses propres cases. La case `appels` de `principal.cf` n'en
sait rien.

```text
module outils.cf                     module principal.cf
+---------------------+              +---------------------+
| VERSION  "1.0"      |  copie à     | double   <func>     |
| appels   1   <------+-- l'import --| VERSION  "1.0"      |
| double   <func>     |   (valait 0) | appels   0          |
| secret   <func>     |              | Point    <struct>   |
+---------------------+              +---------------------+
```

Python fait exactement pareil : `from outils import appels` copie la valeur, et
il faut écrire `import outils` puis `outils.appels` pour lire la valeur à
jour. JavaScript a fait l'autre choix : un `import { appels }` des modules
ES6 est un **lien vivant**, et `appels` suit les changements du module. Ça
demande, pour chaque nom importé, une indirection de plus à chaque lecture.
CeFlo a pris la règle de Python, qui coûte une copie une fois, et rien
ensuite : `double` dans `principal.cf` est un `GET_GLOBAL` comme un autre.

### import *

`OP_IMPORT_ALL` copie tout ce qui est exporté :

```c fichier="src/vm/module.c" lignes="134-152"
// Le `main` d'un module n'empêche pas le fichier qui l'importe d'avoir le sien.
void module_import_all(VM *vm, ObjModule *into, ObjModule *from){
    for(int i = 0; i < from->globals.count; i++){
        ObjString *name = from->names[i];
        if(IS_UNDEFINED(from->globals.values[i]) || name == vm->name_main || !is_exported(from,name))
            continue;
        copy_global(vm,into,from,i);
    }
}

// export :: [a, b] : le fichier ne publie plus que ces noms.
void module_export(VM *vm, ObjModule *module, ObjString *name){
    Value index;
    if(!table_get(&module->indexes,name,&index) || IS_UNDEFINED(module->globals.values[(int)AS_NUMBER(index)]))
        vm_error(vm,"`%s` n'existe pas : on ne peut pas l'exporter",name->chars);

    module->exports = VM_GROW_ARRAY(vm,ObjString*,module->exports,module->export_count,module->export_count + 1);
    module->exports[module->export_count++] = name;
}
```

Trois sortes de cases sont sautées : celles qui ne sont pas encore définies
(un nom seulement lu par le module, ou le module à moitié exécuté de `a.cf`),
le `main` du module (sinon le `main` d'un module importé remplacerait celui du
fichier, et `GRAMMAIRE.md` dit qu'il ne se lance jamais), et ce qui n'est pas
exporté.

`export` est une instruction exécutée, `OP_EXPORT`, et pas une déclaration lue
par le compilateur. Elle vérifie que le nom existe **au moment où elle
s'exécute** : c'est pour ça que `export :: [...]` s'écrit à la fin du fichier,
après les définitions, et que le parser refuse un `export` dans un bloc.

`import *` a un coût caché pour le compilateur. Après lui, n'importe quel nom
peut devenir une globale, puisqu'on ne sait pas ce que le module contient.
Le compilateur pose `has_import_all`, et ne fait plus confiance à sa liste
des builtins : dans `principal.cf`, `println` est un `GET_GLOBAL 3`, pas un
`GET_BUILTIN`. La VM se débrouille à l'exécution, on le verra en 14.4.

### Un import dans une fonction

La forme du milieu de `compile_include` traite un import écrit dans un bloc.
Le module attend dans une case cachée, et chaque nom devient une **locale** :

```ceflo fichier="bloc.cf"
func calcule() {
    from outils import [double]
    return double(5)
}
println(calcule())
println(calcule())
```

```terminal
$ ./Ceflo --dis bloc.cf
...
== calcule ==
0000     2  IMPORT                0  "outils.cf"
0003     |  GET_LOCAL             1
0005     |  IMPORT_VALUE          1  "double"
0008     3  GET_LOCAL             2
0010     |  CONSTANT              2  5
0013     |  CALL                  1
0015     |  RETURN

outils.cf s'exécute
10
10
```

`IMPORT` pose le module dans la case 1. `GET_LOCAL 1` le recopie au sommet,
et `IMPORT_VALUE` remplace cette copie par la valeur de `double`, qui devient
la case 2. Le deuxième appel de `calcule` refait l'`IMPORT`, mais trouve le
module dans `vm->modules` : `outils.cf` ne s'exécute qu'une fois. Un import
de tout dans un bloc, lui, va encore dans les globales : c'est une limite
écrite dans `docs/VM.md`.

## 14.4 Les builtins

`println` n'est défini dans aucun fichier. C'est un **builtin** : une fonction
écrite en C, visible depuis tous les fichiers sans import. On les range dans
une table de la VM, `vm->builtins`, remplie par `vm_init` avant toute
compilation. Chaque entrée est un `ObjNative`, l'objet du chapitre 11 qui
enveloppe un pointeur de fonction C :

```c fichier="src/vm/object.h" lignes="219-229"
    // Une fonction C. Elle reçoit ses arguments et retourne sa valeur.
    // Une erreur s'écrit vm_error(vm, ...) : le programme s'arrête.
    typedef Value (*NativeFn)(struct VM *vm, int argc, Value *args);

    typedef struct{
        Obj obj;
        NativeFn function;
        const char *name;   // un texte qui vit jusqu'à la fin du programme
    }ObjNative;

    ObjNative *native_new(struct VM *vm, const char *name, NativeFn function);
```

La forme d'une native mérite qu'on s'y arrête, parce que c'est celle de toutes
les fonctions C de ce chapitre, jusqu'aux bibliothèques `.so`. Elle reçoit la
VM, le nombre d'arguments, et un pointeur sur le premier argument. Ce pointeur
est **dans la pile de la VM** : on ne copie rien pour appeler une native. Le
cas `OBJ_NATIVE` de `call_value` le montre :

```c fichier="src/vm/vm.c" lignes="176-181"
            case OBJ_NATIVE:{
                Value result = AS_NATIVE(callee)->function(vm,argc,vm->stack_top - argc);
                vm->stack_top -= argc + 1;
                vm_push(vm,result);
                return false;
            }
```

```text
println("a", 1)          [ ...  println  "a"  1 ]
                                         ^ args = stack_top - 2,  argc = 2
après l'appel            [ ...  none ]
```

La native lit `args[0]` et `args[1]`, retourne sa valeur, et la VM remplace
la fonction et ses arguments par le résultat. `return false` dit qu'aucun
`CallFrame` n'a été ouvert : la boucle continue dans la même fonction.

Lua fait la même chose avec sa pile, mais une fonction C y reçoit un
`lua_State` et lit ses arguments avec `lua_tonumber(L, 1)`. CPython passe un
tableau de `PyObject*` depuis Python 3.8, la convention « vectorcall », pour
la même raison que nous : ne pas fabriquer un tuple à chaque appel.

### D'où viennent les builtins

Ils sont déclarés dans `src/vm/natives/core.c`, dans une table qu'on parcourt
une fois au démarrage :

```c fichier="src/vm/natives/core.c" lignes="160-180"
static const core_function Core_List[] = {
    {"println",&core_println},
    {"print",&core_print},
    {"input",&core_input},
    {"format",&core_format},
    {"printf",&core_printf},
    {"nextInt",&core_nextInt},
    {"exit",&core_exit},
    {NULL,NULL}
};

void natives_define_core(VM *vm, int argc, char **argv){
    for(const core_function *item = Core_List; item->name != NULL; item++)
        vm_define_builtin(vm,item->name,item->function);

    // Le GC ne tourne jamais pendant une allocation : la liste n'a pas besoin d'être protégée.
    ObjList *arguments = list_new(vm);
    for(int i = 0; i < argc; i++)
        list_append(vm,arguments,OBJ_VAL(string_new(vm,argv[i],(int)strlen(argv[i]))));
    vm_define_builtin_value(vm,"argv",OBJ_VAL(arguments));
}
```

`argv` est un builtin qui n'est pas une fonction : `vm_define_builtin_value`
range n'importe quelle valeur. Le commentaire est la règle du GC du
chapitre 11 appliquée au démarrage : on alloue la liste, puis chaque chaîne,
sans rien protéger, parce qu'aucun point sûr ne passe entre les deux.

### Ajouter un builtin

On va en ajouter un pour de vrai. `horloge()` retournera le temps de calcul du
programme, pour mesurer un morceau de code depuis CeFlo. J'ai fait le
changement sur une copie de `src/` dans `livre/essais/chapitres-12-14/horloge/`, pour ne
pas toucher au dépôt, et tu peux faire de même. Dans
`src/vm/natives/core.c`, on ajoute `#include <time.h>` en haut, puis la
fonction juste avant la section `$TABLE` :

```c fichier="horloge/src/vm/natives/core.c"
// horloge() : le temps de calcul du programme, en secondes
static Value core_horloge(VM *vm, int argc, Value *args){
    (void)args;
    native_check_args(vm,"horloge",argc,0);
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}
```

Et une ligne dans `Core_List`, juste avant `{NULL,NULL}` :

```c fichier="horloge/src/vm/natives/core.c"
    {"horloge",&core_horloge},
```

`(void)args` fait taire l'avertissement de `-Wextra` sur un paramètre
inutilisé. `native_check_args` vient de `ceflo.h` et arrête le programme avec
un message clair si le nombre d'arguments est faux. `clock()` mesure le temps
de processeur, pas le temps écoulé : pour un programme qui calcule, c'est la
même chose, et c'est dans la bibliothèque standard du C, sans dépendance.

```ceflo fichier="horloge.cf"
func fib(n) {
    if n < 2 { return n }
    return fib(n - 1) + fib(n - 2)
}
var debut = horloge()
println(fib(25))
println(horloge() - debut > 0)
horloge(1)
```

```terminal
$ ./Ceflo_horloge horloge.cf
75025
true
erreur: `horloge` prend 0 argument(s), mais en reçoit 1
  --> horloge.cf:8:8
    8 | horloge(1)
      |        ^
$ ./Ceflo horloge.cf
erreur: `horloge` n'est pas définie
  --> horloge.cf:5:13
    5 | var debut = horloge()
      |             ^
```

Huit lignes, et la fonction existe dans tous les fichiers. Le message d'erreur
pointe sur la parenthèse de l'appel, grâce au noeud que `OP_CALL` a gardé
(chapitre 9). La native n'a rien eu à faire pour ça.

### Un nom du fichier cache un builtin

`GRAMMAIRE.md` dit que les noms des builtins ne sont pas réservés : un script
peut écrire sa propre fonction `println`. Le compilateur doit donc savoir, en
lisant `println`, si le fichier en définit un, même plus bas. C'est la branche
du milieu de `resolve_name` :

```c fichier="src/vm/compiler.c" lignes="484-497" fonction="resolve_name"
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
```

Un nom devient un builtin à trois conditions : pas d'`import *` dans le
fichier, pas de définition du même nom au niveau du fichier, et un builtin de
ce nom existe. `file_names` est remplie avant la compilation par
`scan_file_names` (lignes 358 à 381), qui parcourt les instructions du fichier
sans les compiler. C'est ce qui permet à une fonction écrite tout en bas de
cacher `println` dans tout le fichier :

```ceflo fichier="redef.cf"
func println(x) {
    print("[", x, "]\n")
}
println("bonjour")
```

```terminal
$ ./Ceflo --dis redef.cf
== redef.cf ==
0000     1  CLOSURE               0  <func println>
0003     |  DEFINE_GLOBAL         0
0006     4  GET_GLOBAL            0
0009     |  CONSTANT              1  "bonjour"
0012     |  CALL                  1
0014     |  POP
0015     1  NONE
0016     |  RETURN

== println ==
0000     2  GET_BUILTIN           0  "print"
...
[bonjour]
```

Quand le compilateur ne peut pas savoir (après un `import *`), il compile un
`GET_GLOBAL`, et la VM rattrape à l'exécution. Une globale jamais définie
vaut `UNDEFINED`, et `OP_GET_GLOBAL` tente alors le builtin du même nom :

```c fichier="src/vm/vm.c" lignes="523-529" fonction="undefined_global"
// OP_GET_GLOBAL d'une globale pas encore définie : un builtin du même nom, sinon une erreur.
static Value undefined_global(VM *vm, ObjModule *module, int index){
    Value value;
    if(table_get(&vm->builtins,module->names[index],&value))
        return value;
    vm_error(vm,"`%s` n'est pas définie",module->names[index]->chars);
}
```

C'est le chemin lent, une recherche dans une table à chaque lecture de
`println` après un `import *`. On le paie seulement dans ces fichiers-là.

> [!essaie]
> Ajoute à ta copie un builtin `type_de(x)` qui retourne le nom du type de sa
> valeur, comme les messages d'erreur. Indice : `value_typer_name`, dans
> `src/vm/value.h`, donne déjà ce texte. Il ne reste qu'à en faire une chaîne
> avec `string_from`.

## 14.5 Les méthodes de list, map et str

`liste.append(4)` et `texte.split(",")` ressemblent à des appels de méthodes,
et le compilateur les traite comme tels : c'est un `OP_INVOKE` (chapitre 13).
Mais une liste n'est pas un objet d'une structure, et elle n'a pas de table de
méthodes. On l'a vu au chapitre 13 : `invoke` finit par appeler
`natives_call_method`, dans `src/vm/natives/methods.c`.

Une méthode native a presque la forme d'une native, avec un paramètre de plus,
l'objet :

```c fichier="src/vm/natives/methods.c" lignes="14-19"
typedef Value (*MethodFn)(VM *vm, Value self, int argc, Value *args);

typedef struct{
    const char *name;
    MethodFn function;
}method_t;
```

Il y a une table par type (lignes 261 à 295) : sept méthodes pour une liste,
huit pour un map, une pour une énumération, quatre pour une chaîne. `lenght`
et `length` y sont deux entrées qui pointent sur la même fonction : la faute
de frappe historique de CeFlo (`lenght`) est restée, et l'orthographe juste a
été ajoutée à côté. La recherche, elle, a une petite astuce :

```c fichier="src/vm/natives/methods.c" lignes="297-330" focus="315-321,323-327"
// Le hash de chaque nom, calculé une fois : une méthode se trouve sans strcmp.
#define METHODS_MAX 16

typedef struct{
    const method_t *methods;
    uint32_t hashes[METHODS_MAX];
    int lengths[METHODS_MAX];
    bool ready;
}method_table_t;

static method_table_t Tables[] = {
    {Method_List,{0},{0},false},
    {Method_Map,{0},{0},false},
    {Method_Enum,{0},{0},false},
    {Method_Str,{0},{0},false},
};

static const method_t *method_find(method_table_t *table, ObjString *name){
    if(!table->ready){
        for(int i = 0; table->methods[i].name != NULL; i++){
            table->lengths[i] = (int)strlen(table->methods[i].name);
            table->hashes[i] = string_hash(table->methods[i].name,table->lengths[i]);
        }
        table->ready = true;
    }

    uint32_t hash = string_get_hash(name);
    for(int i = 0; table->methods[i].name != NULL; i++){
        if(table->hashes[i] == hash && table->lengths[i] == name->length &&
           memcmp(table->methods[i].name,name->chars,(size_t)name->length) == 0)
            return &table->methods[i];
    }
    return NULL;
}
```

Les noms des méthodes sont des textes C, pas des chaînes internées : on ne peut
pas les comparer par adresse. Au premier appel, `method_find` calcule le hash
et la longueur de chaque nom de la table, une fois pour toute la durée du
programme. Ensuite, chercher `append` revient à comparer des nombres : le
hash du nom demandé (déjà calculé, puisque c'est une chaîne internée) avec
sept hashes. Le `memcmp` final ne sert qu'à confirmer, et il ne tourne
presque jamais sur un mauvais nom.

On aurait pu ranger ces méthodes dans une `Table` de la VM, avec des clés
internées, comme les méthodes d'une structure. Ce serait un peu plus rapide
pour un type qui en a beaucoup. Avec sept méthodes au plus, la boucle sur un
petit tableau est aussi rapide, et elle reste lisible : ajouter une méthode,
c'est écrire la fonction et une ligne dans la table.

### Une native qui rappelle du CeFlo

La plupart des méthodes ne font qu'un calcul en C. `filter` est différente :
elle appelle la fonction CeFlo qu'on lui donne, pour chaque élément. Et c'est
ici que la règle du GC du chapitre 11 trouve sa seule exception :

```c fichier="src/vm/natives/methods.c" lignes="83-104" fonction="method_list_filter" focus="86-93,102"
static Value method_list_filter(VM *vm, Value self, int argc, Value *args){
    native_check_args(vm,"filter",argc,1);

    // La fonction appelée passe par des points sûrs du GC : tout ce qu'on garde est protégé.
    Value function = args[0];
    Value result = OBJ_VAL(list_new(vm));
    Value item = NONE_VAL;
    vm_protect(vm,&self);
    vm_protect(vm,&function);
    vm_protect(vm,&result);
    vm_protect(vm,&item);

    // Taille relue à chaque tour : la fonction peut modifier la liste.
    for(int i = 0; i < AS_LIST(self)->count; i++){
        item = AS_LIST(self)->items[i];
        if(value_is_true(vm_call(vm,function,1,&item)))
            list_append(vm,AS_LIST(result),item);
    }

    vm_unprotect(vm,4);
    return result;
}
```

`vm_call` exécute du CeFlo, qui passe par des points sûrs : un appel, un
retour, un tour de boucle. Le GC peut alors tourner, et il ne voit que ses
racines. La nouvelle liste `result` n'est sur aucune pile et dans aucune
globale : sans `vm_protect`, elle serait libérée au premier GC, et le
`list_append` suivant écrirait dans de la mémoire rendue. `vm_protect` range
l'adresse de la variable C dans `vm->roots`, que `gc_mark_roots` parcourt.

`self` et `function` sont en fait déjà en sécurité : ce sont des valeurs de la
pile de la VM, posées par l'appelant, et elles y restent pendant tout l'appel.
Les protéger ne coûte rien et évite de devoir y réfléchir. On reverra ce bug,
pour de vrai, avec une bibliothèque `.so` en 14.7.

Le deuxième commentaire est un détail du vrai code : la taille de la liste est
relue à chaque tour, parce que la fonction appelée peut ajouter ou enlever
des éléments. Une boucle qui garderait `count` dans une variable lirait
au-delà de la fin si la fonction vidait la liste.

> [!essaie]
> Ajoute une méthode `map(f)` aux listes, qui retourne une nouvelle liste avec
> `f(x)` pour chaque élément. Copie `method_list_filter`, change deux lignes,
> et ajoute une entrée dans `Method_List`. Teste-la avec une `lfunc` qui
> contient une boucle, pour que le GC ait une chance de tourner, puis compile
> ta copie avec `-DCEFLO_GC_STRESS` et retire un `vm_protect` pour voir ce qui
> arrive.

## 14.6 os et console

`from os import [lire]` passe par `module_import` comme un fichier. Le parser
a fait de `os` le chemin `"os.cf"`, `newLexer` ne trouve aucun fichier de ce
nom, et `natives_standard_module` prend le relais :

```c fichier="src/vm/natives/os.c" lignes="344-382"
// Un module compilé dans l'interpréteur : chaque fonction est une globale, et tout est exporté.
static ObjModule *natives_make_module(VM *vm, const char *path, const native_function *list){
    ObjModule *module = module_new(vm,string_from(vm,path));
    for(const native_function *item = list; item->name != NULL; item++){
        int index = module_global_index(vm,module,string_from(vm,item->name));
        module->globals.values[index] = OBJ_VAL(native_new(vm,item->name,item->function));
    }
    module->exports = NULL;
    module->loaded = true;
    return module;
}

// La table de console.c.
extern const native_function Console_List[];

ObjModule *natives_standard_module(VM *vm, const char *name){
    const native_function *list = NULL;
    const char *path = NULL;

    if(strcmp(name,"os") == 0 || strcmp(name,"os.cf") == 0){
        list = Os_List;
        path = "os.cf";
    }else if(strcmp(name,"console") == 0 || strcmp(name,"console.cf") == 0){
        list = Console_List;
        path = "console.cf";
    }else{
        return NULL;
    }

    // Un module n'est rempli qu'une fois : les imports suivants retrouvent le même.
    ObjString *key = string_from(vm,path);
    Value module;
    if(table_get(&vm->native_modules,key,&module))
        return AS_MODULE(module);

    ObjModule *created = natives_make_module(vm,path,list);
    table_set(&vm->native_modules,key,OBJ_VAL(created));
    return created;
}
```

C'est l'idée qui rend ce morceau si court : un module écrit en C est un
`ObjModule` comme les autres, dont chaque globale est une `ObjNative`. Pour
la suite de la VM, il n'y a aucune différence. `OP_IMPORT_NAME`, `copy_global`
et `find_export` marchent sans changer une ligne, et `from os import *` aussi.
On aurait pu inventer une syntaxe à part, ou un objet `os` avec des attributs.
On a préféré qu'un module fourni par CeFlo ne se distingue en rien d'un module
écrit par l'utilisateur.

Le module est rempli une fois, au premier import, puis gardé dans
`vm->native_modules`. Un script qui n'importe jamais `os` ne paie rien : pas
une `ObjNative` n'est créée.

```ceflo fichier="journal.cf"
from os import [creer_dossier, ecrire, ajouter, lire_lignes, dossier, existe, chemin, commande]

creer_dossier("notes")
var fichier = chemin("notes", "journal.txt")
ecrire(fichier, "lundi : lexer\n")
ajouter(fichier, "mardi : parser\n")

for var i, ligne in lire_lignes(fichier) {
    println(i, " ", ligne)
}
println(dossier("notes"), " ", existe("notes/absent.txt"))
println(lire_lignes("notes/absent.txt"))
var r = commande("wc -l < notes/journal.txt")
println(r.code, " ", r.sortie)
```

```terminal
$ ./Ceflo journal.cf
0 lundi : lexer
1 mardi : parser
[ "journal.txt" ] false
none
0 2

```

La ligne vide à la fin est réelle : la sortie de `wc` se termine par un retour
à la ligne, que `commande` garde.

### Pas d'exceptions

`lire_lignes` d'un fichier absent ne s'arrête pas : il retourne `none`. C'est
la règle écrite en tête de `os.c` : CeFlo n'a pas d'exceptions, et un échec
attendu (un fichier absent, un dossier qu'on ne peut pas créer) est une valeur
que le script teste. Seule une mauvaise utilisation, comme `lire(3)`, arrête
le programme. Les fonctions d'`os.c` suivent donc toutes le même plan, qu'on
voit dans `os_lire` :

```c fichier="src/vm/natives/os.c" lignes="59-69" fonction="os_lire"
static Value os_lire(VM *vm, int argc, Value *args){
    native_check_args(vm,"lire",argc,1);

    buffer_t buffer;
    if(!os_read_file(os_text(vm,"lire",args[0]),&buffer))
        return NONE_VAL;

    ObjString *text = string_new(vm,buffer.data,(int)buffer.size);
    Buffer_Free(&buffer);
    return OBJ_VAL(text);
}
```

D'abord vérifier les arguments, et arrêter le programme s'ils sont faux. Puis
faire l'appel système, et retourner `none` s'il échoue. Enfin fabriquer la
valeur CeFlo. `os_text` (ligne 22) ajoute une vérification qu'on oublierait
sûrement : une chaîne CeFlo peut contenir un `\0`, et un chemin C s'arrête au
premier. `lire("a.txt\0.cf")` ouvrirait `a.txt` sans le dire. `os_text`
refuse ce cas.

Go fait le même choix que CeFlo, avec des valeurs d'erreur retournées en plus
du résultat. Python et Java lèvent des exceptions. Lua fait les deux :
`io.open` retourne `nil` et un message, `error` lève une erreur. Pour un
langage dont la VM doit se lire en un après-midi, ne pas avoir d'exceptions
évite tout un mécanisme : dérouler la pile, fermer les upvalues en chemin,
retrouver le bon `CallFrame`.

### console

`console` est construit de la même façon, avec sa table `Console_List` dans
`src/vm/natives/console.c` : `clrscr`, `getch`, `getchar`, `kbhit` et
`sleep`. C'est le module du jeu du serpent de `exemple/`. Ses fonctions
mettent le terminal en mode brut (sans écho, sans attendre la touche Entrée)
avec `termios`, et un détail du vrai code mérite d'être lu : `console_raw_on`
installe un gestionnaire pour `SIGINT`, pour qu'un Ctrl+C pendant `getch` ne
laisse pas le terminal de l'utilisateur sans écho.

## 14.7 Écrire une bibliothèque .so

Tout ce qu'on a vu jusqu'ici est compilé dans `./Ceflo`. Pour ajouter une
fonction C, il fallait recompiler l'interpréteur, comme pour `horloge`. Une
**bibliothèque partagée** (un `.so` sous Linux, un `.dylib` sous macOS, un
`.dll` sous Windows) permet de charger du code C pendant l'exécution, sans
toucher à l'interpréteur. C'est ce que fait `from @"lib/math/libmath.so" import
[mathSqrt]`. On va en écrire une, la compiler, et la charger.

### Ce qu'inclut une bibliothèque

Une bibliothèque n'inclut qu'un fichier, `src/vm/ceflo.h` :

```c fichier="src/vm/ceflo.h" lignes="16-27"
#ifndef __CEFLO_H__
#define __CEFLO_H__

    #include "vm.h"
    #include "natives.h"

    // Arrêtent le programme avec un message clair si l'appel est mauvais.
    void native_check_args(VM *vm, const char *name, int argc, int expected);
    // expected : "none", "bool", "char", "number", "str", "list", "map", "func", "generic"...
    void native_check_type(VM *vm, const char *name, Value value, const char *expected);

#endif
```

Avec `vm.h`, elle reçoit tout le contrat : `Value` et ses macros, les objets,
`vm_call`, `vm_protect`, `vm_error`. Une fonction exportée a la forme d'une
native : `Value f(VM *vm, int argc, Value *args)`. C'est la troisième
différence voulue de `docs/VM.md` : les bibliothèques de l'ancien
interpréteur avaient une autre forme, et celles de `lib/` ont été réécrites.

La plus petite est `lib/math/math.c`, trois fonctions :

```c fichier="lib/math/math.c" lignes="17-21"
Value mathSqrt(VM *vm, int argc, Value *args){
    native_check_args(vm,"mathSqrt",argc,1);
    native_check_type(vm,"mathSqrt",args[0],"number");
    return NUMBER_VAL(sqrt(AS_NUMBER(args[0])));
}
```

Pas de table, pas de fonction d'enregistrement : le nom de la fonction C est
le nom qu'on importe. La VM le trouvera avec `dlsym`.

### libmots

On écrit la nôtre dans `livre/essais/chapitres-12-14/libmots/mots.c`. Deux fonctions :
l'une transforme un texte, l'autre rappelle une fonction CeFlo. On tape le
début :

```c fichier="libmots/mots.c"
#include <ctype.h>
#include <stdlib.h>

#include "ceflo.h"

// mots_majuscules("abc") : "ABC"
Value mots_majuscules(VM *vm, int argc, Value *args){
    native_check_args(vm,"mots_majuscules",argc,1);
    native_check_type(vm,"mots_majuscules",args[0],"str");

    ObjString *texte = AS_STRING(args[0]);
    ObjString *resultat = string_reserve(vm,texte->length);
    for(int i = 0; i < texte->length; i++)
        resultat->chars[i] = (char)toupper((unsigned char)texte->chars[i]);
    return OBJ_VAL(resultat);
}
```

Ligne par ligne :

- Les deux vérifications d'abord, comme dans `os.c`. Sans elles,
  `mots_majuscules(42)` lirait un nombre comme un pointeur de chaîne.
- `string_reserve` (`object.h`, ligne 104) alloue une chaîne neuve de la bonne
  longueur, avec son `\0` final déjà posé, et nous laisse remplir
  `chars`. On évite ainsi un tampon intermédiaire. Le commentaire de
  `object.h` conseille `string_new` pour un texte court, qui l'interne : ici
  la chaîne n'est pas internée, ce qui est permis (`string_equals` compare
  alors les octets).
- Le `(unsigned char)` n'est pas décoratif : `toupper` reçoit un `int` qui
  doit être positif ou `EOF`, et un octet d'un `é` en UTF-8 est négatif dans un
  `char` signé.

La deuxième fonction applique une fonction CeFlo à chaque élément d'une liste,
comme le `map(f)` de l'exercice précédent :

```c fichier="libmots/mots.c"
// mots_applique(liste, f) : une nouvelle liste, avec f(x) pour chaque x
Value mots_applique(VM *vm, int argc, Value *args){
    native_check_args(vm,"mots_applique",argc,2);
    native_check_type(vm,"mots_applique",args[0],"list");
    native_check_type(vm,"mots_applique",args[1],"func");

    Value liste = args[0];
    Value fonction = args[1];
    Value resultat = OBJ_VAL(list_new(vm));
    vm_protect(vm,&liste);
    vm_protect(vm,&fonction);
    vm_protect(vm,&resultat);

    for(int i = 0; i < AS_LIST(liste)->count; i++){
        Value x = AS_LIST(liste)->items[i];
        list_append(vm,AS_LIST(resultat),vm_call(vm,fonction,1,&x));
    }

    vm_unprotect(vm,3);
    return resultat;
}
```

`"func"` accepte une closure, une native ou une méthode liée
(`native_kind`, ligne 41 de `library.c`). `vm_call` copie ses arguments sur la
pile de la VM : on peut lui passer l'adresse d'une variable C, `&x`.

On compile avec les options du `Makefile` pour `lib/`, depuis
`livre/essais/chapitres-12-14/libmots/` :

```sh
gcc -std=c11 -D_GNU_SOURCE -Wall -Wextra -O2 -fPIC -shared -I../../../src/vm -I../../../src/include -o libmots.so mots.c
```

`-shared` produit une bibliothèque au lieu d'un programme. `-fPIC` (*position
independent code*) produit du code qui marche à n'importe quelle adresse : le
système ne sait pas à l'avance où il chargera la bibliothèque. Les deux `-I`
donnent les en-têtes de la VM, dans cet ordre : `src/vm` passe avant
`src/include`, parce que sur macOS `value.h` et `Value.h` (celui du visitor)
sont le même nom de fichier (`Makefile`, lignes 19 et 20).

```ceflo fichier="libmots/essai.cf"
from @"./libmots.so" import [mots_majuscules, mots_applique]

println(mots_majuscules("bonjour ceflo"))
var carres = mots_applique([1, 2, 3, 4], lfunc(x) { return x * x })
println(carres)
println(mots_applique(["a", "b"], mots_majuscules))
mots_majuscules(42)
```

```terminal
$ ./Ceflo essai.cf
BONJOUR CEFLO
[ 1 , 4 , 9 , 16 ]
[ "A" , "B" ]
erreur: `mots_majuscules` attend un argument de type str, pas number
  --> essai.cf:7:16
    7 | mots_majuscules(42)
      |                ^
```

La troisième ligne passe une native à une native : `mots_applique` appelle
`mots_majuscules` par `vm_call`, qui passe par `call_value` et son cas
`OBJ_NATIVE`. Et l'erreur pointe sur la ligne 7 du script, alors qu'elle est
née dans une fonction C d'une bibliothèque : `vm_error` lit le noeud de
l'instruction en cours, comme partout.

> [!attention]
> J'ai d'abord écrit `from @"libmots.so"`, sans `./`, et obtenu « impossible
> d'ouvrir `libmots.so` : libmots.so: cannot open shared object file ». Un
> chemin sans `/` est cherché par `dlopen` dans les dossiers du système
> (`LD_LIBRARY_PATH`, `/usr/lib`...), jamais dans le dossier courant. Le chemin
> à côté du script n'est essayé que si le script est lui-même dans un dossier.

### Comment la VM charge la bibliothèque

Le compilateur fait de chaque nom importé un `OP_IMPORT_NATIVE`, avec le
chemin et le nom (`compile_include`, lignes 1503 à 1516), puis range la
valeur comme une variable. La VM appelle `natives_load_library`, dans
`src/vm/natives/library.c` :

```c fichier="src/vm/natives/library.c" lignes="113-126" fonction="natives_load_library" focus="117,121-122"
Value natives_load_library(VM *vm, const char *path, ObjString *name){
    void *handle = library_open(vm,path);

    // dlsym retourne un void* : la copie évite une conversion interdite en C.
    void *symbol = dlsym(handle,name->chars);
    if(symbol == NULL)
        vm_error(vm,"`%s` n'existe pas dans `%s`",name->chars,path);

    NativeFn function = NULL;
    memcpy(&function,&symbol,sizeof(function));
    native_names = realloc(native_names,sizeof(char*) * (size_t)(native_name_count + 1));
    native_names[native_name_count] = strdup(name->chars);
    return OBJ_VAL(native_new(vm,native_names[native_name_count++],function));
}
```

`library_open` (lignes 90 à 111) appelle `dlopen` une fois par bibliothèque,
et garde le résultat dans un tableau : importer dix fonctions d'un même `.so`
ne l'ouvre qu'une fois. `dlsym` cherche ensuite la fonction par son nom, et
retourne son adresse.

Les lignes 121 et 122 sont une curiosité du C. `dlsym` retourne un `void*`,
un pointeur vers des données. Le standard C ne permet pas de convertir un
pointeur de données en pointeur de fonction : sur certaines machines, les deux
n'ont même pas la même taille. POSIX garantit que ça marche pour `dlsym`, mais
`gcc -Wpedantic` se plaindrait d'un simple cast. Le `memcpy` copie les octets
sans conversion, et le compilateur le réduit à une affectation.

`native_names` garde une copie du nom, qui vivra jusqu'à la fin du programme.
`ObjNative` ne garde qu'un `const char*`, et `name->chars` appartient à une
chaîne que le GC peut libérer. `natives_free` libère ces copies et ferme les
bibliothèques à la fin, après les objets.

### -rdynamic

Une question doit te travailler depuis le début de la section :
`libmots.so` appelle `native_check_args` et `list_new`, qui sont dans `./Ceflo`.
Comment la bibliothèque les trouve-t-elle ? Elle ne les contient pas, et on
ne l'a liée à rien. Par défaut, un programme n'expose pas ses fonctions aux
bibliothèques qu'il charge. Pour le voir, j'ai compilé la VM sans l'option qui
change ça :

```terminal
$ ../Ceflo_sans_rdynamic essai.cf
../Ceflo_sans_rdynamic: symbol lookup error: ./libmots.so: undefined symbol: native_check_args
```

L'option est dans `LDFLAGS`, ligne 15 du `Makefile` :

```text fichier="Makefile" lignes="15-15"
LDFLAGS = -rdynamic -lm -ldl
```

`-rdynamic` demande à l'éditeur de liens de mettre toutes les fonctions du
programme dans sa table de symboles dynamiques. Au `dlopen`, les symboles
manquants de la bibliothèque sont cherchés là. Sous macOS, l'éditeur de liens
refuse même de produire une bibliothèque qui a des symboles inconnus : il
faut lui dire `-undefined dynamic_lookup` (`Makefile`, lignes 49 à 52).
`-ldl` apporte `dlopen` et `dlsym` sur les anciennes versions de la glibc.

C'est le modèle de Lua et de Python : une extension C appelle les fonctions de
l'interpréteur qui la charge. L'autre modèle, celui de Node.js avec son API
N-API, passe une table de pointeurs de fonctions à l'extension, qui n'a alors
besoin d'aucun symbole. C'est plus portable, et beaucoup plus de code à
écrire.

### Le bug qu'on aurait fait

Il reste à vérifier les trois `vm_protect` de `mots_applique`. J'ai compilé
une version de la VM où le GC tourne à **chaque** point sûr
(`-DCEFLO_GC_STRESS`, ce que fait `make sanitize`), avec AddressSanitizer
pour attraper les accès à de la mémoire rendue. Puis une copie de `mots.c`
sans les quatre lignes de protection, `libmots_sans.so`. Pour que le GC ait
l'occasion de tourner, la fonction passée contient une boucle :

```ceflo fichier="libmots/gc.cf"
from @"./libmots_sans.so" import [mots_applique]

func triple(x) {
    var total = 0
    for var i = 0; i < 3; i += 1 { total += x }
    return total
}
println(mots_applique([1, 2, 3], triple))
```

```terminal
$ ../Ceflo_stress gc.cf
=================================================================
==436821==ERROR: AddressSanitizer: heap-use-after-free on address 0x503000000538 at pc 0x634a39374b36 bp 0x7ffed4e28680 sp 0x7ffed4e28670
READ of size 4 at 0x503000000538 thread T0
    #0 0x634a39374b35 in list_append src/vm/object.c:90
    #1 0x71b2a26d5785 in mots_applique (libmots_sans.so+0x1785) (BuildId: 688e574f907d882257adafd58e9947b478102ef2)
...
freed by thread T0 here:
    #0 0x71b2a20fc4d8 in free ../../../../src/libsanitizer/asan/asan_malloc_linux.cpp:52
    #1 0x634a3934b8e3 in pool_release src/common.c:61
    #2 0x634a3936c901 in object_free src/vm/memory.c:138
    #3 0x634a39370a32 in gc_sweep src/vm/memory.c:339
...
    #6 0x634a39391f2f in vm_run src/vm/vm.c:846
```

Tout y est : la liste `resultat` a été libérée par `gc_sweep`, depuis le point
sûr de `OP_LOOP` (ligne 846 de `vm.c`) dans la boucle de `triple`, et
`list_append` l'a lue ensuite. Avec la version protégée, la même VM affiche
`[ 3 , 6 , 9 ]`. Sans le GC à chaque point sûr, ce bug ne se serait montré
qu'un jour, sur une grosse liste, au moment où le GC aurait décidé de tourner.

Avec une `lfunc` simple comme `x * x`, le bug ne se montre même pas en mode
stress : une fonction sans boucle ni appel retourne avant tout point sûr
(`OP_RETURN` d'une boucle `vm_run` relancée par `vm_call` sort avant son
`SAFEPOINT`). C'est la raison pour laquelle un test doit appeler du code qui
boucle.

> [!essaie]
> Ajoute à `libmots` une fonction `mots_compte(texte, carac)` qui retourne le
> nombre de fois où le caractère apparaît. Un `char` CeFlo se vérifie avec
> `"char"` et se lit avec `AS_CHAR`. Compile, importe, et vérifie qu'un
> appel avec une chaîne à la place du caractère donne un message clair.

## 14.8 Ce qu'on a construit

Fais le compte. Un lexer, un parser à descente récursive et un arbre, communs
aux deux interpréteurs : 1 913 lignes de C. Un interpréteur qui parcourt
l'arbre, avec ses portées, son GC et ses modules : 3 514 lignes. Une machine
virtuelle complète, avec son compilateur, son GC, ses closures, ses objets,
ses modules et toutes ses natives : 7 413 lignes. Aucune dépendance, sinon la
bibliothèque standard du C et `dlopen`.

```terminal
$ tests/run.sh ./Ceflo
...
218 réussis, 0 échoués
```

Et la vitesse. Voici `make bench`, lancé une seule fois au début de
l'écriture de ce chapitre, sur ma machine (le meilleur de trois essais pour
chaque programme, et la sortie comparée à celle de l'ancien interpréteur) :

```terminal
$ make bench
bench/run.sh ./Ceflo ./Ceflo_ref
                    VM    ancien    Python   vs ancien   vs Python
boucle            0.37      2.14      0.88        5.8x        2.4x
closures          0.10      0.62      0.41        6.2x        4.1x
fib               0.09      0.50      0.16        5.6x        1.8x
gros_texte        1.99      5.63      0.15        2.8x        0.1x
listes            0.10      1.69      0.37       16.9x        3.7x
maps              0.07      0.46      0.20        6.6x        2.9x
methodes          0.18      0.80      0.40        4.4x        2.2x
mots              0.10      0.65      0.20        6.5x        2.0x
objets            0.22      0.69      0.34        3.1x        1.5x
texte             0.82      1.06      0.92        1.3x        1.1x
```

Le chapitre 9 commençait sur un constat : le visitor était trois fois plus
lent que Python. La VM est maintenant plus rapide que Python sur neuf
programmes sur dix, de 1,1 fois sur `texte` à 4,1 fois sur `closures`. Elle va
de 1,3 à 16,9 fois plus vite que le visitor. `fib(30)`, 2,7 millions d'appels
de fonction, prend 0,09 s.

Chaque ligne du tableau se relie à un chapitre. `listes` (16,9 fois) est le
gain des valeurs de 16 octets du chapitre 9 : plus aucun nombre n'est alloué.
`closures` (6,2 fois, et 4,1 fois Python) mesure les upvalues du chapitre 12 :
un compteur appelé un million de fois lit et écrit sa variable par un
pointeur. `methodes` et `objets` mesurent le chapitre 13 : les attributs à
place fixe et `OP_INVOKE`. `boucle` et `fib` mesurent le chapitre 11, la
boucle d'exécution et ses `computed goto`.

### Ce qui reste à faire

La ligne `gros_texte` est la seule où Python gagne, et de loin : 1,99 s contre
0,15 s. C'est la dernière limite de `docs/VM.md`. Le programme construit un
texte de 30 000 lignes avec `texte += l + "\n"`. Une chaîne CeFlo ne change
jamais : chaque `+=` crée une nouvelle chaîne et recopie tout le texte
d'avant. Le travail croît comme le carré du nombre de lignes. CPython triche
intelligemment : quand une chaîne n'a qu'une seule référence (il le sait grâce
à son comptage de références), il l'agrandit sur place. Java et Lua ont le
même problème que CeFlo, et le résolvent dans la bibliothèque, avec
`StringBuilder` et `table.concat`. CeFlo pourrait faire l'un ou l'autre : une
méthode `join` sur les listes, ou un objet `buffer`. C'est le premier défi de
ce chapitre.

Le reste de la liste est dans `docs/VM.md` et dans ces quatorze chapitres :
une locale déclarée plus bas qu'une `lfunc` qui la lit (chapitre 12), le
`match` qui ne sait pas reconnaître une énumération à la compilation
(chapitre 13), un `import *` dans un bloc qui va dans les globales. Et ce que
CeFlo n'a pas du tout : des exceptions, des bibliothèques sous Windows, un
débogueur pas à pas, un client HTTP. Aucune de ces choses ne demande de
nouvelle idée. Tu as déjà vu chacune des pièces qu'elles utiliseraient.

## 14.9 Écrire ton propre langage

Tu as lu un langage entier, du premier caractère lu par `lexer_advance`
jusqu'au `dlsym` d'une bibliothèque. Pour écrire le tien, tu n'as pas besoin
de refaire le chemin dans le même ordre que CeFlo. CeFlo a été écrit deux
fois, d'abord en visitor, puis en bytecode, parce que c'est ainsi qu'il a
vraiment été construit. Toi, tu peux choisir. Voici l'ordre que je
conseillerais, avec la taille de chaque étape dans CeFlo pour te donner une
idée.

1. **La grammaire, sur une page.** Avant toute ligne de C, écris ton
   `GRAMMAIRE.md`. Celui de CeFlo tient en une centaine de lignes pour les règles. Chaque
   règle deviendra une fonction du parser : si la grammaire est confuse, le
   parser le sera.
2. **Le lexer et le parser, jusqu'à afficher l'arbre.** C'est 1 900 lignes
   dans CeFlo, messages d'erreur compris. Ne passe pas à la suite tant que
   l'arbre affiché n'est pas exactement celui que tu attends, et tant que
   chaque erreur de syntaxe ne pointe pas le bon caractère.
3. **Un visitor, pour avoir un langage qui marche.** C'est le chemin le plus
   court vers un programme qui tourne, et il te servira de référence pour
   toujours, comme `Ceflo_ref` sert à `make diff`. Garde-le petit.
4. **Les tests, tout de suite.** Un dossier de `.cf` et de `.out`, et un
   script de trente lignes comme `tests/run.sh`. Chaque bug corrigé devient un
   test. Les 218 tests de CeFlo ont permis de réécrire tout l'intérieur sans
   casser le langage.
5. **Le bytecode, si tu as besoin de vitesse.** Les quatre idées de
   `docs/VM.md` font presque tout le gain : une valeur qui se copie, des noms
   résolus à la compilation, des attributs à place fixe, un GC qui ne tourne
   qu'aux points sûrs. Commence par les deux premières.

Quelques conseils, que ce livre a appris à ses dépens :

- **Écris le désassembleur avant la VM.** `--dis` a servi à chaque chapitre de
  la partie 3. Un bug de compilateur se voit dans le bytecode bien avant
  qu'il ne fasse planter la VM.
- **Fais tourner le GC tout le temps pendant les tests.** Le bug de la section
  14.7 aurait attendu des mois. `CEFLO_GC_STRESS` le trouve en une seconde.
- **Garde les erreurs aussi bonnes que le langage.** Un message qui dit la
  ligne, la colonne et ce qui était attendu vaut plus qu'une fonctionnalité
  de plus. Chaque octet de bytecode de CeFlo garde son noeud pour ça.
- **Copie sans honte.** La pile de CeFlo, ses upvalues et son `OP_INVOKE`
  viennent de Lua et de *Crafting Interpreters*. Ses modules ressemblent à
  ceux de Python. Ce qui fait un langage, c'est l'ensemble des choix, pas
  l'originalité de chaque pièce.

Si tu veux aller plus loin, trois lectures. *Crafting Interpreters* de Robert
Nystrom, en accès libre sur craftinginterpreters.com, construit deux fois le
même langage, comme ce livre. Le code source de Lua 5.4, environ 30 000
lignes de C très lisibles, montre une VM à registres au lieu d'une pile.
*Engineering a Compiler* de Cooper et Torczon, pour le jour où tu voudras
optimiser le code produit.

Et surtout, commence petit. Le premier programme de ton langage n'a pas
besoin de structures ni de modules. Il a besoin d'afficher `3` quand on écrit
`1 + 2`. Tout le reste de CeFlo est venu après.

## Défis

1. **`join`.** Ajoute aux listes une méthode `join(sep)` qui assemble des
   chaînes en une seule, avec une seule allocation : calcule d'abord la
   longueur totale, puis remplis une chaîne de `string_reserve`. Réécris
   `bench/gros_texte.cf` avec `lignes.join("\n")` (dans une copie) et relance
   `bench/run.sh` sur ce seul programme. Regarde `method_list_filter` pour la
   forme, et `format` dans `vm.c` pour le remplissage.

2. **Un module `temps`.** Fais de `horloge` un module à part,
   `from temps import [horloge, date]`, comme `os` et `console`. Il faut une
   table `Temps_List`, une branche dans `natives_standard_module`, et rien
   d'autre dans la VM. `date()` peut retourner un map
   `{ annee, mois, jour }` construit avec `map_new` et `map_set`.

3. **`import outils` sans `from`.** Ajoute la forme `import outils`, qui donne
   le module lui-même dans une variable, et `outils.double(2)`. Le module doit
   alors répondre à `OP_GET_FIELD` et à `OP_INVOKE` : ajoute un cas
   `IS_MODULE` dans `get_field_slow` et dans `invoke`, qui lit la globale
   exportée. Tu auras un lien vivant : `outils.appels` suivra les changements.
   Il faut aussi toucher le parser (`parser_parse_include`) et la grammaire.

4. **Des bibliothèques qui s'enregistrent.** Aujourd'hui, chaque nom importé
   d'un `.so` est cherché avec `dlsym`. Fais que `library_open` cherche aussi
   une fonction `ceflo_init(VM *vm)` dans la bibliothèque, et l'appelle si
   elle existe : la bibliothèque pourrait y définir des builtins avec
   `vm_define_builtin`, ou préparer un état. Pense à ce qui se passe si deux
   fichiers importent la même bibliothèque.

## Note de conception

Ce chapitre a ouvert CeFlo au C de trois façons : les builtins compilés dans
l'interpréteur, les modules `os` et `console` qui ressemblent à des fichiers,
et les bibliothèques `.so` chargées à la demande. Toutes les trois reposent
sur une seule forme de fonction, `Value f(VM *vm, int argc, Value *args)`, et
c'est le choix de ce chapitre qui compte le plus.

Une interface C est un contrat qu'on ne peut plus changer. Le jour où des
gens auront écrit des bibliothèques pour ton langage, chaque changement de
`Value` ou de `vm_call` les cassera. Python porte encore les choix de son API
C des années 1990 : le comptage de références qu'on ne peut pas retirer, le
verrou global qu'on a mis vingt ans à rendre facultatif. Lua a choisi
l'inverse : une bibliothèque ne voit jamais une valeur Lua, seulement des
index dans une pile, et l'intérieur de la VM a pu changer complètement d'une
version à l'autre sans casser les extensions.

CeFlo a fait le choix de Python, et je pense que c'était le bon choix pour
lui. Une bibliothèque CeFlo lit `AS_STRING(args[0])->chars` directement, sans
une couche d'appels à traverser, et une bibliothèque de quarante lignes comme
`libmots` se lit sans rien connaître d'autre que `object.h`. Le prix est
écrit en tête de `docs/VM.md` : les bibliothèques de l'ancien interpréteur ont
dû être réécrites. Un langage qui a peu d'utilisateurs peut se permettre de
casser son contrat. Un langage qui en a beaucoup ne le peut plus, et c'est
pourquoi les grands langages paraissent si prudents.

Si tu écris ton propre langage, pose-toi la question tôt, avant la première
bibliothèque écrite par quelqu'un d'autre. Et fais comme CeFlo : un seul
en-tête à inclure, `ceflo.h`, qui dit exactement ce qu'une bibliothèque a le
droit d'utiliser. Tout ce qui n'y est pas pourra changer.
