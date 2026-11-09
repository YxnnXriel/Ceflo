---
numero: 9
titre: les instructions
note: une valeur de 16 octets
partie: Le bytecode
---

## 9.1 Trois fois plus lent que Python

À la fin du chapitre 8, CeFlo marche. `make test-ref` passe, les exemples
tournent, le GC ne perd rien. On a écrit un langage. Alors on lui donne un
petit travail, le calcul que tout le monde fait pour tester un langage : la
suite de Fibonacci, écrite de la façon la plus naïve possible.

```ceflo fichier="bench/fib.cf"
// Appels de fonction récursifs
func fib(n){
    if n < 2 { return n }
    return fib(n - 1) + fib(n - 2)
}
println(fib(30))
```

`fib(30)` fait 2 692 537 appels de fonction. On chronomètre :

```terminal
$ time ./Ceflo_ref bench/fib.cf
832040

real	0m0,504s
user	0m0,503s
sys	0m0,001s
```

Une demi-seconde. Ça paraît raisonnable, jusqu'à ce qu'on lance le même
programme en Python, qui est lui aussi un langage interprété :

```terminal
$ time python3 bench/fib.py
832040

real	0m0,177s
user	0m0,172s
sys	0m0,005s
```

Python va presque trois fois plus vite. Et ce n'est pas un hasard de ce
programme-là. Le dépôt contient dix petits programmes de mesure dans `bench/`,
chacun écrit en CeFlo et en Python, et `make bench` les lance tous :

```terminal
$ make bench
bench/run.sh ./Ceflo ./Ceflo_ref
                    VM    ancien    Python   vs ancien   vs Python
boucle            0.31      2.15      0.77        6.9x        2.5x
closures          0.11      0.70      0.42        6.4x        3.8x
fib               0.09      0.52      0.17        5.8x        1.9x
gros_texte        2.07      5.32      0.16        2.6x        0.1x
listes            0.13      1.76      0.43       13.5x        3.3x
maps              0.08      0.47      0.18        5.9x        2.2x
methodes          0.19      0.82      0.40        4.3x        2.1x
mots              0.11      0.66      0.20        6.0x        1.8x
objets            0.23      0.69      0.43        3.0x        1.9x
texte             0.92      1.17      0.84        1.3x        0.9x
```

Regarde d'abord les colonnes « ancien » et « Python ». « ancien », c'est le
visitor des chapitres 5 à 8, compilé sous le nom `Ceflo_ref`. Il perd sur
chaque ligne. Une boucle qui additionne des nombres : 2,15 s contre 0,77 s. Un
million d'ajouts dans une liste : 1,76 s contre 0,43 s.

Regarde maintenant la première colonne. C'est `./Ceflo`, la machine virtuelle
qu'on va construire dans ce chapitre et les deux suivants. Elle bat l'ancien
interpréteur sur toutes les lignes, et Python sur huit lignes sur dix. Le
lexer, le parser et l'arbre ne changent pas d'un octet : tout se joue après.

Pour chaque programme, `bench/run.sh` garde le meilleur temps sur trois essais.
Il fait aussi une chose qu'on oublie souvent quand on mesure : il compare la
sortie de la VM et celle de Python à la sortie de `Ceflo_ref`. Si elles
diffèrent, la case affiche `faux` au lieu d'un temps. Un programme rapide qui
donne un mauvais résultat ne compte pas. L'ancien interpréteur reste donc
compilé à côté de la VM : il est lent, mais c'est lui qui dit ce qui est
juste.

> [!note]
> Les deux lignes où Python garde l'avantage, `gros_texte` et `texte`,
> construisent un long texte avec `+=`. Ce n'est pas une question de vitesse
> de la boucle. On verra au chapitre 11 pourquoi Python triche un peu sur ce
> cas, et pourquoi CeFlo ne peut pas faire pareil.

## 9.2 Où part le temps

Avant de réécrire quoi que ce soit, on veut savoir ce que fait le visitor de
son temps. On prend `bench/boucle.cf` et on le fait tourner 100 000 fois au
lieu de dix millions, pour pouvoir le passer à la loupe :

```ceflo fichier="boucle_petit.cf"
// Arithmétique et variables locales dans une boucle
func somme(n){
    var total = 0
    for var i = 0; i < n; i += 1 {
        total += i % 7
    }
    return total
}
println(somme(100000))
```

La loupe s'appelle `callgrind`. C'est un outil de valgrind qui fait tourner le
programme sur un processeur simulé et compte chaque instruction machine
exécutée :

```terminal
$ valgrind --tool=callgrind --callgrind-out-file=ref.out ./Ceflo_ref boucle_petit.cf 2>&1 | grep refs
==178395== I   refs:      181,080,773
```

181 millions d'instructions du processeur pour 100 000 tours de boucle. Le
démarrage du programme en coûte moins de 400 000 : il reste donc à peu près
**1 800 instructions machine par tour**. Pour comparer un entier à `n`, faire un
modulo, deux additions. Un programme C ferait ce tour en une dizaine.

`callgrind_annotate --tree=caller ref.out` dit où vont ces instructions, et
surtout combien de fois chaque fonction est appelée. Deux chiffres sautent aux
yeux : `makeNumber` est appelée 500 003 fois, et `strcmp` 500 345 fois. Cinq
fois par tour, chacune.

### Cinq nombres par tour

Le premier chiffre vient d'ici. Quand le visitor rencontre un nombre écrit dans
le code, il fabrique une valeur :

```c fichier="src/visitor.c" lignes="1029-1029" fonction="visitor_dispatch"
        case AST_NUMBER: return makeNumber(node->number_value);
```

```c fichier="src/Value.c" lignes="45-49" fonction="makeNumber"
value_t *makeNumber(double x){
    value_t *value = newValue(NUMBER);
    AS_NUMBER(value) = x;
    return value;
}
```

```c fichier="src/Value.c" lignes="13-17" fonction="newValue"
static value_t *newValue(type_ceflo type){
    value_t *value = gc_alloc(sizeof(*value),GC_VALUE);
    value->type = type;
    return value;
}
```

Chaque nombre est un objet du GC. Compte ceux d'un tour : le `7` écrit dans
`i % 7`, le résultat du modulo, le résultat de `total + ...`, le `1` écrit dans
`i += 1`, et le nouveau `i`. Cinq allocations, cinq objets que le GC devra
ensuite marquer ou libérer. Le résultat de `i < n` ne compte pas : `makeBool`
rend toujours l'un des deux booléens partagés.

Le visitor ne peut pas faire autrement. Une valeur, chez lui, c'est un
`value_t *`, un pointeur. Les fonctions se passent des pointeurs, les portées
rangent des pointeurs, les listes contiennent des pointeurs. Un nombre doit
donc vivre quelque part en mémoire, et quelqu'un doit le libérer.

### Cinq noms par tour

Le second chiffre vient de la lecture des variables :

```c fichier="src/visitor.c" lignes="114-122" fonction="visitor_find_variable"
static hash_t *visitor_find_variable(visitor_t *visitor, ast_t *node){
    if(node->variable_hash == 0)
        node->variable_hash = Hash_Name(node->variable_name);

    hash_t *item = scope_lookup_hashed(visitor->scope,node->variable_name,node->variable_hash);
    if(item == NULL)
        visitor_error(node,"`%s` n'est pas définie",node->variable_name);
    return item;
}
```

```c fichier="src/scope.c" lignes="20-27" fonction="scope_lookup_hashed"
hash_t *scope_lookup_hashed(scope_t *scope, const char *name, uint32_t hash){
    for(; scope != NULL; scope = scope->scope_parent){
        hash_t *item = Hash_Get_Hashed(&scope->variables,name,hash);
        if(item != NULL)
            return item;
    }
    return NULL;
}
```

Le hash est déjà gardé dans le noeud (c'était une première optimisation), mais
il reste à remonter la chaîne des portées, à trouver la case dans chaque table,
et à comparer le nom avec `strcmp` pour être sûr. Dans un tour, le visitor lit
`i` et `n` pour la condition, `total` et `i` pour l'addition, `i` pour
l'incrément. Cinq noms, cinq recherches, cinq `strcmp`.

Or le nom `i` désigne toujours la même variable. On le sait en lisant le
programme, avant même de le lancer. Le visitor, lui, le redécouvre un demi-million
de fois.

### Le reste

Il reste le parcours de l'arbre lui-même. Pour `total += i % 7`, le visitor
appelle `visitor_visit` sur le noeud d'affectation, qui l'appelle sur le `%`,
qui l'appelle sur `i` puis sur `7`. Chaque appel passe par le grand `switch` de
`visitor_dispatch`, suit des pointeurs vers des noeuds éparpillés dans la
mémoire, et remonte. C'est la part la plus grosse de la mesure : près de 30 %
des instructions sont dans `visitor_visit` lui-même.

On a donc trois problèmes, et ils donnent les trois idées de la suite :

1. une valeur qui se copie comme un `int`, sans allocation (ce chapitre) ;
2. des noms traduits une fois pour toutes en numéros de case, avant
   l'exécution (le compilateur, chapitre 10) ;
3. une suite plate d'instructions à la place de l'arbre, lue par une simple
   boucle (ce chapitre et le chapitre 11).

Le chemin d'un programme gagne une étape :

::: etapes
texte -> jetons -> arbre -> bytecode -> résultat
:::

Le **bytecode**, c'est cette suite d'instructions. On l'appelle ainsi parce que
chaque instruction tient dans un octet. La **machine virtuelle** (la VM) est le
programme qui l'exécute. On commence par ce qu'elle manipule : ses valeurs.

## 9.3 Une valeur de 16 octets

Toute la nouvelle version vit dans un dossier à part, `src/vm/`, à côté du
visitor qu'on ne touche pas. Le lexer, le parser et l'arbre (`src/lexer.c`,
`src/parser.c`, `src/ast.c`) sont partagés : les deux interpréteurs lisent le
même arbre. Le `Makefile` compile les deux ensemble, `./Ceflo` pour la VM et
`./Ceflo_ref` pour le visitor, et c'est ce qui rend possible le tableau de
9.1.

Dans ce chapitre, on écrit les fichiers dont tous les autres dépendent, dans
cet ordre : `value.h` (la valeur), le début de `object.h` (l'en-tête des
objets), `chunk.h` et `chunk.c` (le bytecode), puis `debug.c` (le
désassembleur). Les définitions des objets eux-mêmes (`ObjString`,
`ObjFunction`...) remplissent le reste de `object.h` au fil des chapitres
suivants. On commence par la valeur, dans `src/vm/value.h` :

```c fichier="src/vm/value.h" lignes="24-50" focus="35-43"
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
```

C'est la même idée que `value_t` au chapitre 5 : une étiquette (`type`) et une
union. La différence est ailleurs : `Value` n'est pas un pointeur. C'est une
petite structure qu'on passe et qu'on rend **par copie**, comme un `int`.
`NUMBER_VAL(3)` construit une `Value` sur place, dans une variable locale du C
ou dans une case d'un tableau. Aucun `malloc`, rien à libérer, rien que le GC
ait à suivre.

Combien d'octets ? Le plus gros membre de l'union est le `double` (8 octets).
L'étiquette est un `enum`, donc un `int` de 4 octets, et le compilateur C
ajoute 4 octets de remplissage pour que le `double` soit aligné sur 8. Un
petit programme d'essai le confirme, avec l'ancienne valeur à côté :

```c fichier="tailles.c"
// Combien d'octets pour une valeur, avant et après ?
#include <stdio.h>
#include "Value.h"

int main(void){
    printf("value_t (le visitor) : %zu octets\n",sizeof(value_t));
    return 0;
}
```

```terminal
$ gcc -Isrc/include -o tailles tailles.c && ./tailles
value_t (le visitor) : 40 octets
$ gcc -Isrc/vm -o tailles_vm tailles_vm.c && ./tailles_vm
Value (la VM) : 16 octets
```

(`tailles_vm.c` est le même programme avec `#include "value.h"` et
`sizeof(Value)`.) L'ancienne valeur fait 40 octets, parce que son union
contient de grosses structures (`Obj_t`, `Func_t`...) et qu'elle commence par
l'en-tête du GC. Surtout, ces 40 octets vivent sur le tas, et le programme ne
manipule que leur adresse. La nouvelle valeur fait 16 octets et vit là où on
la pose.

Regarde ce que ça change pour une addition. Dans le visitor, `a + b` sur deux
nombres finissait par `makeNumber(x + y)` : un appel à `gc_alloc`, qui
cherche une place dans un réservoir, l'inscrit dans la liste du GC, remplit
l'en-tête, et rend un pointeur. Plus tard, le GC devra marquer cette valeur,
puis la libérer. Dans la VM, le même calcul s'écrit
`NUMBER_VAL(AS_NUMBER(a) + AS_NUMBER(b))`. Le compilateur C le traduit par
une addition de deux `double` et deux écritures : l'étiquette `VAL_NUMBER`
et le résultat, directement dans la case de la pile où il doit aller. Pas
d'appel, pas de liste, rien que le GC verra jamais.

Le prix, c'est la copie. Passer une `Value` à une fonction copie 16 octets au
lieu de 8 pour un pointeur. Sur les processeurs actuels, une structure de 16
octets passe dans deux registres, et cette copie ne coûte presque rien. Un
`value_t` de 40 octets, lui, ne pourrait pas être copié aussi facilement :
c'est une autre raison pour laquelle le visitor manipulait des pointeurs.

### Ce qui reste sur le tas

Une chaîne, une liste, une fonction ne tiennent pas dans 8 octets. Pour elles,
la valeur garde un `Obj *`, et l'objet lui-même est alloué par le GC. Chaque
objet commence par le même en-tête, dans `object.h` :

```c fichier="src/vm/object.h" lignes="43-48"
    struct Obj{
        ObjType type;
        bool marked;
        bool printing;   // en cours d'affichage : une liste qui se contient s'écrit "..."
        struct Obj *next;
    };
```

Le type de l'objet (`OBJ_STRING`, `OBJ_LIST`, `OBJ_CLOSURE`...), la marque pour
le mark and sweep, et le lien vers l'objet alloué juste avant, pour que le GC
puisse tous les parcourir. Une `ObjString` commence par un `Obj`, puis sa
longueur, son hash et ses caractères. Un pointeur vers une `ObjString` est donc
aussi un pointeur vers un `Obj` : c'est ce qui permet de ranger n'importe quel
objet dans le champ `obj` de l'union.

On a donc deux mondes. none, bool, char et number sont des valeurs
**immédiates** : elles sont dans la `Value`. Tout le reste est un objet, et la
`Value` n'en garde que l'adresse. C'est exactement le partage qu'il fallait
pour la boucle de la section précédente : les cinq nombres de chaque tour ne
quittent plus jamais la pile.

`VAL_UNDEFINED` est une valeur que le script ne voit jamais. Elle marque une
case qui existe mais n'a pas encore reçu de valeur : une globale déclarée plus
bas dans le fichier, par exemple. On la retrouvera au chapitre 10.

> [!note]
> On peut aller plus loin et faire tenir une valeur dans 8 octets : un
> `double` a des milliards de motifs « NaN » inutilisés, assez pour y cacher un
> pointeur ou un booléen. Ça s'appelle le NaN boxing, et LuaJIT le fait. C'est
> plus rapide encore, mais chaque accès devient un jeu de masques binaires.
> CeFlo garde la version qu'on lit sans effort.

## 9.4 Une machine à pile

On a des valeurs. Il faut maintenant une façon d'écrire des instructions qui
les manipulent. Le choix de CeFlo est celui de Python, de Java et de la
plupart des langages de script : une **machine à pile**. Le commentaire en
tête de `chunk.h` le dit en quelques lignes :

```c fichier="src/vm/chunk.h" lignes="5-14"
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
```

La pile est un simple tableau de `Value`, avec un pointeur vers la première
case libre. Pousser une valeur, c'est l'écrire là et avancer le pointeur.
`ADD` ne dit pas quoi additionner : il prend les deux valeurs du sommet, les
retire, et pose la somme à leur place.

Le mieux est de le voir sur un vrai programme. `./Ceflo --dis` affiche le
bytecode de chaque fonction avant de l'exécuter :

```ceflo fichier="plus_un.cf"
func plus_un(a){
    return a + 1
}
println(plus_un(41))
```

```terminal
$ ./Ceflo --dis plus_un.cf
== plus_un.cf ==
0000     1  CLOSURE               0  <func plus_un>
0003     |  DEFINE_GLOBAL         0
0006     4  GET_BUILTIN           1  "println"
0009     |  GET_GLOBAL            0
0012     |  CONSTANT              2  41
0015     |  CALL                  1
0017     |  CALL                  1
0019     |  POP
0020     1  NONE
0021     |  RETURN

== plus_un ==
0000     2  GET_LOCAL             1
0002     |  CONSTANT              0  1
0005     |  ADD
0006     |  RETURN

42
```

Le haut du fichier et la fonction `plus_un` ont chacun leur bytecode. Chaque
ligne est une instruction : l'endroit où elle commence dans le code (l'offset,
en octets), la ligne du source, son nom, ses opérandes. On reviendra sur le
format exact en 9.7.

Suis la ligne 4 du source, `println(plus_un(41))`, en tenant la pile à la main
(le sommet est à droite) :

```text
GET_BUILTIN 1   pile : println
GET_GLOBAL 0    pile : println plus_un
CONSTANT 2      pile : println plus_un 41
CALL 1          pile : println 42
CALL 1          pile : none
POP             pile :
```

Le premier `CALL 1` appelle la fonction posée sous son unique argument :
`plus_un`, avec 41. Le résultat prend la place de la fonction et de ses
arguments. Le second `CALL 1` fait pareil avec `println` et 42, qui affiche
`42` et rend `none`. Le `POP` final jette ce `none` : `println(...)` est une
instruction, sa valeur ne sert à rien.

Dans `plus_un`, le paramètre `a` est la case 1 (la case 0 est réservée, on
verra pourquoi au chapitre 10). `GET_LOCAL 1` le pousse, `CONSTANT 0` pousse
le `1`, `ADD` les remplace par leur somme, `RETURN` rend le sommet à
l'appelant.

Une machine à pile a un grand avantage pour la suite : traduire une
expression devient presque mécanique. Pour `a + b`, on traduit `a`, puis `b`,
puis on écrit `ADD`. Chaque morceau laisse sa valeur au sommet, et l'opérateur
trouve ses deux opérandes exactement où il les attend. Les parenthèses
disparaissent d'elles-mêmes :

```terminal
$ ./Ceflo --dis calcul.cf
== calcul.cf ==
0000     1  GET_BUILTIN           0  "println"
0003     |  CONSTANT              1  2
0006     |  CONSTANT              2  3
0009     |  CONSTANT              3  4
0012     |  ADD
0013     |  MULTIPLY
0014     |  CONSTANT              4  1
0017     |  SUBTRACT
0018     |  CALL                  1
0020     |  POP
0021     |  NONE
0022     |  RETURN

13
```

C'est `println(2 * (3 + 4) - 1)`. Le `ADD` passe avant le `MULTIPLY` parce que
l'arbre le voulait : `3 + 4` est un fils de `*`. L'ordre des instructions est
l'ordre de l'arbre lu de gauche à droite, chaque opérateur après ses deux
fils.

> [!note]
> Lua 5 a fait l'autre choix, une machine à registres. On en discute dans la
> note de conception, à la fin du chapitre.

## 9.5 Le chunk

Le bytecode d'une fonction est rangé dans un **chunk** (un « morceau » ; le nom
vient de *Crafting Interpreters*, le livre de Robert Nystrom, dont la VM de
CeFlo reprend beaucoup de noms). C'est une structure de `chunk.h` :

```c fichier="src/vm/chunk.h" lignes="144-153"
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
```

Quatre choses :

- `code` : les octets du bytecode, `count` utilisés sur `capacity` réservés ;
- `nodes` : pour chaque octet de `code`, le noeud de l'arbre qui l'a produit ;
- `constants` : les valeurs écrites dans le code (nombres, chaînes, fonctions) ;
- `checks` : les vérifications de type, qu'on verra au chapitre 10.

### Écrire un octet

```c fichier="src/vm/chunk.c" lignes="26-35" fonction="chunk_write"
void chunk_write(Chunk *chunk, uint8_t byte, ast_t *node){
    if(chunk->count == chunk->capacity){
        chunk->capacity = chunk->capacity < 8 ? 8 : chunk->capacity * 2;
        chunk->code = ceflo_realloc(chunk->code,(size_t)chunk->capacity);
        chunk->nodes = ceflo_realloc(chunk->nodes,sizeof(ast_t*) * (size_t)chunk->capacity);
    }
    chunk->code[chunk->count] = byte;
    chunk->nodes[chunk->count] = node;
    chunk->count += 1;
}
```

Un tableau qui double quand il est plein, comme le `buffer_t` du chapitre 1.
Les deux tableaux grandissent ensemble : l'octet `i` de `code` a toujours son
noeud en `nodes[i]`.

Une instruction est un octet, l'**opcode**, suivi de ses opérandes. Un
opérande `u8` tient dans un octet (0 à 255), un `u16` dans deux (0 à 65 535),
l'octet de poids fort d'abord. Dans `plus_un`, `GET_LOCAL 1` occupe les
octets 0 et 1, `CONSTANT 0` les octets 2, 3 et 4 : c'est pour ça que `ADD`
commence à l'offset 5. Le numéro d'une case locale est un `u8` (256 locales
par fonction au plus), le numéro d'une constante un `u16` (65 536 constantes).

### Pourquoi un octet, et pourquoi poids fort d'abord

Un opcode d'un octet laisse de la place pour 256 instructions ; CeFlo en
utilise 65. Les opérandes suivent l'opcode, et leur nombre dépend de
l'instruction : `ADD` tient en un octet, `CONSTANT` en trois, `ADD_LOCAL` en
quatre. On appelle ça un codage à **longueur variable**. C'est celui de la
machine virtuelle de Java, dont le nom même, « bytecode », vient de là.

D'autres VM ont choisi une longueur fixe. Depuis Python 3.6, chaque
instruction de CPython fait exactement deux octets, un pour l'opcode et un
pour l'argument (les Python appellent ça le « wordcode »). Lua range chaque
instruction dans un mot de 32 bits. L'avantage d'une longueur fixe est qu'on
trouve l'instruction suivante sans rien décoder, et qu'on peut lire tout le
mot d'une seule lecture mémoire. L'inconvénient est la place perdue :
`ADD` occuperait deux octets ou quatre au lieu d'un, et un numéro de constante
plus grand que 255 demande, chez CPython, une instruction spéciale
`EXTENDED_ARG` placée devant.

CeFlo garde la longueur variable parce qu'elle se lit et s'écrit le plus
simplement : un octet après l'autre, sans masques. Un `u16` laisse 65 536
constantes et 65 536 globales par fichier, ce qu'aucun script raisonnable
n'atteint, et le compilateur vérifie quand même la limite (chapitre 10).

L'ordre des deux octets d'un `u16` est un choix arbitraire, pourvu que le
compilateur et la VM fassent le même. Les processeurs x86 et ARM rangent les
entiers poids faible d'abord ; CeFlo écrit le poids fort d'abord, comme les
fichiers `.class` de Java et comme les protocoles réseau. L'avantage est
uniquement humain : dans un vidage d'octets, `00 0E` se lit 14 sans calcul.
Comme on lit les deux octets un par un, l'ordre de la machine n'intervient
jamais.

### Les constantes

Un nombre comme `41` ne peut pas tenir dans un opérande d'un octet : c'est un
`double` de 8 octets. Il va donc dans le tableau `constants` du chunk, et
`CONSTANT 2` veut dire « pousse la constante numéro 2 ».

```c fichier="src/vm/chunk.c" lignes="37-53" fonction="chunk_add_constant"
// Deux nombres sont la même constante s'ils ont les mêmes bits : 0 et -0 restent distincts.
static bool chunk_same_constant(Value a, Value b){
    if(IS_NUMBER(a) && IS_NUMBER(b))
        return memcmp(&AS_NUMBER(a),&AS_NUMBER(b),sizeof(double)) == 0;
    return IS_STRING(a) && IS_STRING(b) && AS_OBJ(a) == AS_OBJ(b);
}

int chunk_add_constant(Chunk *chunk, Value value){
    if(IS_NUMBER(value) || IS_STRING(value)){
        for(int i = 0; i < chunk->constants.count; i++){
            if(chunk_same_constant(chunk->constants.values[i],value))
                return i;
        }
    }
    value_array_write(&chunk->constants,value);
    return chunk->constants.count - 1;
}
```

Une même constante n'est rangée qu'une fois. Les nombres se comparent bit à
bit avec `memcmp` : `0 == -0` est vrai en C, mais ce ne sont pas les mêmes
bits, et le compilateur ne doit pas remplacer l'un par l'autre. Les chaînes se
comparent par adresse, parce que les chaînes du code sont **internées** : deux
textes égaux sont le même objet (on le verra au chapitre 11).

Les noms aussi sont des constantes. Dans le désassemblage de `plus_un.cf`,
`"println"` est la constante 1. Un programme qui appelle deux fois `println`
n'a qu'une constante `"println"`, on va le voir dans un instant.

### Le noeud de chaque octet

`nodes` est la pièce la plus coûteuse du chunk : 8 octets de pointeur pour
chaque octet de code. Elle sert à une seule chose, et c'est une chose à
laquelle on tient. Prends ce programme :

```ceflo fichier="division.cf"
var x = 10
var y = 0
println("x vaut ", x)
println(x + 1, " ", x / y)
```

```terminal
$ ./Ceflo division.cf
x vaut 10
erreur: division par zéro
  --> division.cf:4:23
    4 | println(x + 1, " ", x / y)
      |                       ^
```

C'est exactement le message de `Ceflo_ref`, au caractère près, avec la flèche
sous le `/`. Le bytecode de la ligne 4 montre comment la VM s'y retrouve :

```terminal
$ ./Ceflo --dis division.cf
== division.cf ==
0000     1  CONSTANT              0  10
0003     |  DEFINE_GLOBAL         0
0006     2  CONSTANT              1  0
0009     |  DEFINE_GLOBAL         1
0012     3  GET_BUILTIN           2  "println"
0015     |  CONSTANT              3  "x vaut "
0018     |  GET_GLOBAL            0
0021     |  CALL                  2
0023     |  POP
0024     4  GET_BUILTIN           2  "println"
0027     |  GET_GLOBAL            0
0030     |  CONSTANT              4  1
0033     |  ADD
0034     |  CONSTANT              5  " "
0037     |  GET_GLOBAL            0
0040     |  GET_GLOBAL            1
0043     |  DIVIDE
0044     |  CALL                  3
0046     |  POP
0047     1  NONE
0048     |  RETURN

x vaut 10
erreur: division par zéro
  --> division.cf:4:23
    4 | println(x + 1, " ", x / y)
      |                       ^
```

(Le `"println"` de la ligne 4 réutilise bien la constante 2.) La division est
l'octet 43. Quand `DIVIDE` échoue, la VM lit `nodes[43]` : c'est le noeud
`AST_BINOP` du `/`, celui que le parser a construit au chapitre 3, avec son
fichier, sa ligne 4 et sa colonne 23. Elle le passe à la même fonction
d'affichage que le visitor, qui ressort la ligne du source et place la
flèche. Le compilateur a jeté l'arbre en tant que programme, mais il garde
chaque noeud en tant qu'adresse d'erreur.

> [!note]
> *Crafting Interpreters* ne garde que le numéro de ligne de chaque octet.
> CeFlo garde le noeud entier, parce que les messages du visitor
> donnaient la colonne et la flèche, et qu'on ne voulait pas qu'un programme
> plus rapide affiche des erreurs moins précises.

## 9.6 Soixante-cinq instructions

L'`enum OpCode` de `chunk.h` contient 65 instructions. C'est le contrat entre
le compilateur, qui les écrit, et la VM, qui les exécute. Chaque ligne dit ses
opérandes et ce qu'elle fait à la pile, sous la forme `[avant] -> [après]`,
le sommet à droite. On ne les apprend pas par coeur : on les lit par famille.

Les valeurs et les variables :

```c fichier="src/vm/chunk.h" lignes="31-49"
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
```

Regarde les opérandes des variables. Aucune ne porte de nom : `GET_LOCAL`
prend un numéro de case, `GET_GLOBAL` un numéro de globale. C'est la deuxième
idée de 9.2, écrite dans le format même des instructions. Il n'y a plus de
place pour une recherche par nom pendant l'exécution, parce que le nom n'est
plus là. Seul `GET_BUILTIN` garde un nom, rangé en constante : les builtins
(`println`, `input`...) sont peu nombreux et vivent dans une table de la VM.

Les opérateurs suivent, de `OP_ADD` à `OP_TO_BOOL` : un par opérateur de
CeFlo, tous de la forme `[a b] -> [a op b]` ou `[a] -> [op a]`. Puis les sauts,
qui donnent les `if` et les boucles :

```c fichier="src/vm/chunk.h" lignes="72-83"
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
```

`ip` est le pointeur d'instruction : l'adresse du prochain octet à lire. Un
saut ne fait que le déplacer. `JUMP` avance toujours, `JUMP_IF_FALSE` avance
si la valeur du sommet est fausse, `LOOP` recule. Il n'y a pas d'instruction
`if` ni `while` : le compilateur construira tout avec ces trois-là.

Viennent ensuite les familles qu'on ne fera qu'effleurer ici, parce que
chacune a son chapitre. Les listes, les maps et les accès (`OP_LIST`,
`OP_GET_INDEX`, `OP_GET_FIELD`...). Les structures et les énumérations
(`OP_STRUCT`, `OP_METHOD`, `OP_NEW`...), au chapitre 13. Les boucles
`for ... in` et `match` (`OP_ITER_PREP`, `OP_ITER_NEXT`, `OP_MATCH_FAIL`), au
chapitre 10. Les modules (`OP_IMPORT`, `OP_EXPORT`...), au chapitre 14. Les
upvalues (`OP_GET_UPVALUE`, `OP_CLOSURE`, `OP_CLOSE_UPVALUE`) ont le
chapitre 12 pour elles seules.

### Les instructions rapides

La dernière famille est différente. Elle n'ajoute rien au langage :

```c fichier="src/vm/chunk.h" lignes="121-126"
        // ======== instructions rapides ========
        // Des raccourcis pour les cas fréquents : le compilateur les choisit quand ils sont sûrs.
        OP_SET_LOCAL_POP,   // u8 slot         [a] -> []           slots[slot] = a
        OP_ADD_LOCAL,       // u8 slot, u16 k  [] -> []            slots[slot] = slots[slot] + constants[k]   (x += 1)
        OP_GET_SELF_FIELD,  // u8 i            [] -> [self.fields[i]]   dans une méthode : self est slots[0]
        OP_SET_SELF_FIELD,  // u8 i            [v] -> []           sans vérification : le compilateur s'en charge
```

Chacune remplace une suite d'instructions que les programmes écrivent sans
arrêt. Prends `x += 1` sur une variable locale. Sans raccourci,
c'est `GET_LOCAL x`, `CONSTANT 1`, `ADD`, puis `SET_LOCAL_POP x` (qui est
déjà un raccourci pour `SET_LOCAL` suivi de `POP`) : quatre tours de la boucle
de la VM. `ADD_LOCAL x 1` fait le même travail en un seul tour.

Pourquoi ne pas avoir écrit uniquement les instructions rapides ? Parce
qu'elles ne marchent que dans des cas précis. `ADD_LOCAL` n'est valable que si
`x` est une locale, sans type déclaré, et si ce qu'on ajoute est une constante.
Le compilateur choisit la version rapide quand il peut prouver que c'est sûr,
et la version longue sinon. La section suivante montre les deux côte à côte
dans la même boucle.

## 9.7 Lire le bytecode

Le désassembleur est le meilleur ami de quelqu'un qui écrit une VM. Quand un
programme donne un résultat faux, on regarde son bytecode avant de regarder le
C. Il tient dans `src/vm/debug.c`, et il commence par un tableau de noms :

```c fichier="src/vm/debug.c" lignes="22-30"
static const char *opcode_names[OP_COUNT] = {
    [OP_CONSTANT] = "CONSTANT",
    [OP_NONE] = "NONE",
    [OP_TRUE] = "TRUE",
    [OP_FALSE] = "FALSE",
    [OP_POP] = "POP",
    [OP_DUP] = "DUP",
    [OP_DUP2] = "DUP2",
    [OP_GET_LOCAL] = "GET_LOCAL",
```

La syntaxe `[OP_CONSTANT] = "CONSTANT"` (un initialiseur désigné, en C99)
range chaque nom à la case de son opcode, quel que soit l'ordre des lignes.
Si quelqu'un ajoute une instruction au milieu de l'`enum`, les noms ne se
décalent pas. Une instruction oubliée reste à `NULL`, et `opcode_name`
affiche alors `???` au lieu de planter.

Ensuite, une ligne par instruction :

```c fichier="src/vm/debug.c" lignes="203-213" fonction="chunk_disassemble_instruction"
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
```

L'offset, puis la ligne du source, lue dans le noeud de l'octet : voilà
`nodes` qui sert une seconde fois. Une barre `|` veut dire « même ligne que
l'instruction d'avant », pour que les lignes du source se détachent. Le
`switch` qui suit range chaque instruction selon la forme de ses opérandes, et
rend l'offset de l'instruction suivante : `CONSTANT` en occupe 3, `GET_LOCAL`
2, `ADD` 1.

Les sauts ont droit à un petit calcul :

```c fichier="src/vm/debug.c" lignes="146-151" fonction="jump_operand"
// Un saut affiche aussi l'offset où il arrive.
static int jump_operand(Chunk *chunk, OpCode op, int sign, int offset){
    int distance = read_u16(chunk,offset + 1);
    printf("%-18s %4d  -> %04d\n",opcode_name(op),distance,offset + 3 + sign * distance);
    return offset + 3;
}
```

La distance se compte depuis l'octet qui suit le saut (`offset + 3`), vers
l'avant ou vers l'arrière selon `sign`. Lire `JUMP_IF_FALSE 14` ne dit rien ;
lire `-> 0028` dit où on atterrit.

### Une vraie boucle

On a maintenant tout ce qu'il faut pour lire le bytecode d'une boucle, ligne
par ligne :

```ceflo fichier="boucle.cf"
func somme(n){
    var total = 0
    var i = 0
    while i < n {
        total += i
        i += 1
    }
    return total
}
println(somme(10))
```

```terminal
$ ./Ceflo --dis boucle.cf
== boucle.cf ==
0000     1  CLOSURE               0  <func somme>
0003     |  DEFINE_GLOBAL         0
0006    10  GET_BUILTIN           1  "println"
0009     |  GET_GLOBAL            0
0012     |  CONSTANT              2  10
0015     |  CALL                  1
0017     |  CALL                  1
0019     |  POP
0020     1  NONE
0021     |  RETURN

== somme ==
0000     2  CONSTANT              0  0
0003     3  CONSTANT              0  0
0006     4  GET_LOCAL             3
0008     |  GET_LOCAL             1
0010     |  LESS
0011     |  JUMP_IF_FALSE        14  -> 0028
0014     5  GET_LOCAL             2
0016     |  GET_LOCAL             3
0018     |  ADD
0019     |  SET_LOCAL_POP         2
0021     6  ADD_LOCAL             3    1  1
0025     4  LOOP                 22  -> 0006
0028     8  GET_LOCAL             2
0030     |  RETURN

45
```

Dans `somme`, `n` est la case 1, `total` la case 2, `i` la case 3. Lis-le
comme un programme en assembleur :

- **0000 et 0003.** `var total = 0` et `var i = 0` poussent chacun un `0`. Il
  n'y a pas d'instruction pour « créer une variable » : la valeur poussée
  reste sur la pile, et sa case devient la variable. Les deux `0` sont la même
  constante, la numéro 0.
- **0006 à 0011.** La condition `i < n`. Si elle est fausse, `JUMP_IF_FALSE`
  saute à 0028 : 11 + 3 + 14 = 28, la sortie de la boucle.
- **0014 à 0019.** `total += i` en version longue : `total`, `i`, `ADD`, et
  `SET_LOCAL_POP 2` range le résultat dans la case 2.
- **0021.** `i += 1` en une seule instruction : case 3, constante 1 (qui vaut
  `1`). C'est la version rapide, parce qu'on ajoute une constante. Pour
  `total += i`, on ajoute une variable, et le compilateur a pris la version
  longue.
- **0025.** `LOOP 22` recule à 0006 : 25 + 3 − 22 = 6, le début de la
  condition. Sa ligne est 4, celle du `while`, parce que c'est le noeud du
  `while` qui l'a écrit.
- **0028 et 0030.** `return total`.

Une boucle `while` est donc devenue un test, un saut en avant pour sortir, et
un saut en arrière pour recommencer. La VM ne sait pas qu'elle exécute une
boucle : elle avance dans les octets, et de temps en temps quelqu'un change
`ip`.

Dernière remarque, les `NONE` et `RETURN` à la fin du haut du fichier. Le code
d'un fichier est compilé comme une fonction sans argument, et une fonction se
termine toujours par un `RETURN`. Leur ligne est 1 parce qu'ils appartiennent
au noeud du programme entier, qui commence à la ligne 1.

Quant au désassembleur lui-même, il affiche le chunk du fichier, puis cherche
dans ses constantes les fonctions qu'il contient, et les affiche à leur tour :

```c fichier="src/vm/module.c" lignes="26-34" fonction="disassemble_function"
// `--dis` : la fonction, puis chaque fonction rangée dans ses constantes.
static void disassemble_function(ObjFunction *function){
    chunk_disassemble(&function->chunk,function->name != NULL ? function->name->chars : function->module->path->chars);
    for(int i = 0; i < function->chunk.constants.count; i++){
        Value constant = function->chunk.constants.values[i];
        if(IS_FUNCTION(constant))
            disassemble_function(AS_FUNCTION(constant));
    }
}
```

C'est pour ça que `== somme ==` apparaît après `== boucle.cf ==` : `somme` est
la constante 0 du fichier, celle que pousse `CLOSURE 0`.

## 9.8 Un chunk écrit à la main

On a lu beaucoup de bytecode produit par `./Ceflo`. Pour être sûr qu'on a
compris le format, on va en écrire un nous-mêmes, sans compilateur, et le
donner au désassembleur. C'est le premier programme qui utilise les fichiers
de ce chapitre, et il compile sans rien d'autre de la VM. Crée `essai_chunk.c`
dans un dossier d'essai :

```c fichier="essai_chunk.c"
// Un chunk écrit à la main, puis désassemblé : -(1.2 + 3.4) / 5.6
#include <stdio.h>
#include "chunk.h"

int main(void){
    Chunk chunk;
    chunk_init(&chunk);

    int k = chunk_add_constant(&chunk,NUMBER_VAL(1.2));
    chunk_write(&chunk,OP_CONSTANT,NULL);
    chunk_write(&chunk,(uint8_t)(k >> 8),NULL);
    chunk_write(&chunk,(uint8_t)k,NULL);

    k = chunk_add_constant(&chunk,NUMBER_VAL(3.4));
    chunk_write(&chunk,OP_CONSTANT,NULL);
    chunk_write(&chunk,(uint8_t)(k >> 8),NULL);
    chunk_write(&chunk,(uint8_t)k,NULL);

    chunk_write(&chunk,OP_ADD,NULL);
```

Chaque `CONSTANT` s'écrit en trois octets : l'opcode, puis le numéro de la
constante en deux octets, le poids fort (`k >> 8`) d'abord. Le cast
`(uint8_t)k` garde l'octet de poids faible : c'est la troncature du C qui
fait le travail d'un `& 0xFF`. On passe `NULL` comme noeud : il n'y a pas de
source derrière ce chunk. La fin du `main` suit la même logique :

```c fichier="essai_chunk.c"
    k = chunk_add_constant(&chunk,NUMBER_VAL(5.6));
    chunk_write(&chunk,OP_CONSTANT,NULL);
    chunk_write(&chunk,(uint8_t)(k >> 8),NULL);
    chunk_write(&chunk,(uint8_t)k,NULL);

    chunk_write(&chunk,OP_DIVIDE,NULL);
    chunk_write(&chunk,OP_NEGATE,NULL);
    chunk_write(&chunk,OP_RETURN,NULL);

    chunk_disassemble(&chunk,"essai");
    chunk_free(&chunk);
    return 0;
}
```

C'est le bytecode de `-((1.2 + 3.4) / 5.6)` : l'addition, puis la division,
puis le signe. Pour compiler, il faut `chunk.c` et `debug.c`, plus les
fichiers dont ils se servent : `value.c` pour le tableau des constantes,
`common.c` pour `ceflo_realloc`, et quelques autres que `value.c` et
`object.h` attirent avec eux :

```terminal
$ gcc -std=c11 -D_GNU_SOURCE -Wall -Wextra -Isrc/vm -Isrc/include -o essai_chunk essai_chunk.c src/vm/chunk.c src/vm/debug.c src/vm/value.c src/vm/object.c src/vm/table.c src/vm/memory.c src/common.c src/error.c src/str.c -lm
$ ./essai_chunk
== essai ==
0000     0  CONSTANT              0  1.2
0003     0  CONSTANT              1  3.3999999999999999
0006     0  ADD
0007     0  CONSTANT              2  5.5999999999999996
0010     0  DIVIDE
0011     0  NEGATE
0012     0  RETURN
```

Treize octets, sept instructions. La colonne des lignes vaut 0 partout : c'est
le `node != NULL ? node->line : 0` de `chunk_disassemble_instruction`. Sans
ce test, le désassembleur lirait `NULL->line` et planterait : un chunk sans
noeud doit rester affichable, ne serait-ce que pour ce genre d'essai.

Et puis il y a ce `3.3999999999999999`. Ce n'est pas un bug. `3.4` n'a pas
de représentation exacte en binaire : le `double` le plus proche vaut
3,399999999999999911182158029987... Le désassembleur l'affiche avec `%.17g`,
dix-sept chiffres significatifs, parce que c'est le nombre de chiffres qui
garantit qu'on peut relire la valeur et retrouver exactement les mêmes bits.
`println` ferait l'inverse, et afficherait `3.4` : pour l'utilisateur, la
valeur courte ; pour celui qui débogue la VM, la valeur exacte. Python a le
même dilemme, et sa fonction `repr` choisit le plus court texte qui se relit
à l'identique.

### Le tableau des constantes

`chunk_add_constant` range les valeurs avec `value_array_write`, le tableau qui
grandit de `src/vm/value.c` :

```c fichier="src/vm/value.c" lignes="18-25" fonction="value_array_write"
void value_array_write(ValueArray *array, Value value){
    if(array->count == array->capacity){
        array->capacity = array->capacity < 8 ? 8 : array->capacity * 2;
        array->values = ceflo_realloc(array->values,sizeof(Value) * (size_t)array->capacity);
    }
    array->values[array->count] = value;
    array->count += 1;
}
```

La même forme que `chunk_write` : 8 cases au départ, puis on double. Doubler
plutôt qu'ajouter un nombre fixe de cases, c'est ce qui rend l'ajout rapide en
moyenne. Sur un million d'ajouts, il n'y a que dix-sept `realloc`, et
chaque valeur n'est recopiée qu'une ou deux fois au total. Une croissance de
« 8 cases de plus » ferait 125 000 `realloc`, et recopierait le tableau entier
à chaque fois.

`ceflo_realloc` (dans `src/common.c`) est un `realloc` qui ne rend jamais
`NULL` : sans mémoire, il affiche `erreur: mémoire insuffisante` et arrête le
programme. Personne dans la VM n'a donc à tester le résultat d'une
allocation.

On sait maintenant lire le bytecode, et même l'écrire. On sait ce que veut
dire chaque octet, et pourquoi la VM n'aura plus jamais à chercher un nom ni à
allouer un nombre. Mais écrire ces octets à la main, pour un vrai programme,
personne n'en a envie. Entre l'arbre du chapitre 4 et la
suite plate qu'on vient de lire, il manque un programme : celui qui sait que
`total` est la case 2, qui calcule le `14` du `JUMP_IF_FALSE` avant de
connaître la fin de la boucle, et qui choisit `ADD_LOCAL` pour `i += 1` mais
pas pour `total += i`. C'est le compilateur, et c'est le chapitre suivant.

## Défis

1. **Sur papier.** Écris le bytecode de `var a = 2 * (3 + 4)` puis de
   `println(a - 1)`, avec les offsets et les numéros de constantes, puis
   vérifie avec `./Ceflo --dis`. Fais-le ensuite pour `boucle.cf` en
   remplaçant le `while` par un `for` : où passe le `ADD_LOCAL` ?

2. **Les noms des globales.** Fais afficher le nom de la globale à côté de
   `GET_GLOBAL`, `SET_GLOBAL` et `DEFINE_GLOBAL` (`GET_GLOBAL 0  plus_un`). Le
   chunk ne connaît pas les noms, mais `disassemble_function`
   (`src/vm/module.c`) a la fonction, et `function->module->names`
   (`src/vm/object.h`) les garde tous. Il faudra passer le module jusqu'à
   `chunk_disassemble_instruction` (`src/vm/debug.c`).

3. **Le calcul d'avance.** `calcul.cf` calcule `2 * (3 + 4) - 1` à chaque
   exécution, alors que le résultat est connu avant. Le compilateur fait déjà
   un seul calcul à l'avance : devine lequel, puis vérifie dans
   `compile_unop` (`src/vm/compiler.c`). Le chapitre 10 propose de le
   généraliser.

4. **Compter soi-même.** Lance `callgrind` sur `bench/listes.cf` avec
   `./Ceflo_ref` (réduis d'abord le million à 10 000), puis
   `callgrind_annotate --tree=caller` : combien d'appels à `makeNumber` pour
   chaque élément ajouté, lu, puis soustrait ? Explique chacun en lisant
   `src/visitor.c`.

## Note de conception

### Pile ou registres

CeFlo exécute son bytecode sur une pile. `a + b` devient trois instructions :
pousser `a`, pousser `b`, additionner. Lua, depuis sa version 5.0 (2003), fait
autrement : sa VM a des **registres**, des cases numérotées de la fonction, et
`a + b` devient une seule instruction, `ADD R3 R1 R2`, qui lit deux cases et
écrit dans une troisième. Les locales de Lua sont directement des registres :
`x = a + b` ne déplace rien.

Sur le papier, les registres gagnent. Moins d'instructions, c'est moins de
tours de boucle de dispatch, et on verra au chapitre 11 que ce tour coûte
cher. Dans leur article sur Lua 5.0, ses auteurs montrent que le passage aux
registres a rendu leurs programmes de test nettement plus rapides. Python, Java, .NET,
WebAssembly sont pourtant restés des machines à pile. Pourquoi ?

Parce que la pile simplifie tout le reste. Le compilateur d'une machine à pile
n'a rien à décider : chaque expression laisse sa valeur au sommet, et
l'opérateur la trouve là. Avec des registres, il faut choisir une case pour
chaque résultat intermédiaire, la libérer quand elle ne sert plus, et éviter
d'écraser une variable vivante. C'est l'**allocation de registres**, un vrai
sujet de compilation. Les instructions sont aussi plus longues : `ADD R3 R1
R2` porte trois opérandes, là où `ADD` n'en a aucun. Le bytecode de Lua tient
chaque instruction dans 32 bits, au prix d'un format plus serré à décoder.

Pour CeFlo, le choix s'est fait sur un critère simple : il fallait qu'on
puisse lire le compilateur en entier. Le chapitre 10 montrera qu'avec une
pile, `compile_binop` fait trois lignes. Et la pile garde une grande part de
l'avantage des registres sur le point qui compte le plus : les variables
locales sont des cases fixes, lues par numéro, sans aucune recherche. Le
reste, les instructions rapides comme `ADD_LOCAL` le grignotent là où ça
compte. Si un jour CeFlo veut aller encore plus vite, passer aux registres
sera une vraie option. Pour un premier langage, je crois que c'est la pile
qu'il faut écrire.

