---
numero: 6
titre: les portées
note: qui se souvient de x ?
partie: Exécuter l'arbre
---

Avant de lire la sortie, décide ce que ce programme doit afficher :

```ceflo fichier="laquelle.cf"
var x = "globale"

func montre() {
    println(x)
}

func appelle() {
    var x = "locale"
    montre()
}

appelle()
```

```terminal
$ ./Ceflo_ref laquelle.cf
globale
```

Au moment où `montre` s'exécute, il existe deux `x`. Celui du fichier vaut `"globale"`. Celui de `appelle` vaut `"locale"`, et `appelle` est encore en train de tourner : c'est lui qui a appelé `montre`. CeFlo choisit le premier. Bash, ou les premiers Lisp, auraient choisi le second.

Ce choix n'est pas un détail. Il décide si une fonction peut se souvenir de ses variables après la fin de la fonction qui l'a créée, ce qui fait marcher le compteur du `README`. Il décide aussi de ce qu'on doit allouer à chaque appel, à chaque bloc, à chaque tour de boucle, et donc d'une bonne part de la vitesse de l'interpréteur.

Au chapitre 5, on a fabriqué des valeurs. Elles flottent quelque part dans la mémoire, et personne ne sait encore les retrouver par leur nom. Il faut une table qui associe `x` à sa valeur, et une règle qui dit laquelle consulter. C'est une **portée** : les noms visibles à un endroit du programme.

### Comment on va travailler

Ce chapitre écrit la table de noms à la fin de `src/list.c`, puis `src/include/scope.h` et `src/scope.c`, et enfin `src/typer.c`, les types déclarés. Comme au chapitre 5, il n'y a pas encore de visitor. On reprend le `provisoire.c` du chapitre 5, et on lui ajoute la seule fonction que `typer.c` demande en plus, `visitor_new_object`, pour `@Point(1, 2)` :

```c fichier="livre/essais/chapitres-05-08/ch6/provisoire6.c"
// Le provisoire du chapitre 5, plus ce que typer.c demande au visitor.
#include "visitor.h"
#include "error.h"

value_t *visitor_new_object(visitor_t *visitor, ast_t *node, value_t *structure, value_t **args, int argc){
    (void)visitor;(void)structure;(void)args;(void)argc;
    visitor_error(node,"pas encore de structures");
}
```

```sh fichier="livre/essais/chapitres-05-08/ch6/compile.sh"
#!/bin/sh
# ./compile.sh essai : les valeurs, les portées et les types, sans visitor ni GC
SRC=../../../src
gcc -std=c11 -D_GNU_SOURCE -Wall -Wextra -g -I$SRC/include -o $1 $1.c ../ch5/provisoire.c provisoire6.c \
    $SRC/Value.c $SRC/value_expr.c $SRC/map.c $SRC/scope.c $SRC/typer.c $SRC/ast.c \
    $SRC/str.c $SRC/list.c $SRC/common.c $SRC/error.c $SRC/token.c -lm
```

`ast.c` est là pour `GetFunctionInStruct`, que `typer.c` utilise pour trouver un `__cast__`. Les programmes CeFlo du chapitre tournent avec `./Ceflo_ref`, comme au chapitre 5.

## 6.1 Une table de noms

Le chapitre 1 a signalé une table de hachage dans `src/list.c`, en disant qu'elle attendrait le chapitre 6. La voici. Une case de la table, c'est un nom, sa valeur, et deux renseignements qu'on expliquera en 6.4 :

```c fichier="src/include/list.h" lignes="20-35"
    // Table de hachage : nom -> valeur.
    // Les clés ne sont pas copiées : elles doivent vivre aussi longtemps que la table.
    #define TAB_SIZE 8

    typedef struct HASH_STRUCT{
        const char *key;
        void *value;
        const char *type_name;  // type déclaré avec `::`, ou NULL
        bool is_const;
        uint32_t hash;
        struct HASH_STRUCT *next;
    }hash_t;

    typedef struct TABLE_STRUCT{
        hash_t *TabHash[TAB_SIZE];
    }table_t;
```

C'est la table de hachage la plus simple qui soit : un tableau de huit listes chaînées. Le hash d'un nom choisit une des huit cases, et la case contient la liste de tous les noms tombés là. Ce n'est pas la table du map (chapitre 5), qui grandit et garde l'ordre. Ici, on ne grandit jamais.

La clé n'est pas copiée. `key` pointe vers le texte du nom tel qu'il est rangé dans l'arbre, dans `variable_definition_name` d'un noeud `var`. L'arbre ne change jamais et vit jusqu'à la fin du programme (chapitre 4), donc ce texte aussi. C'est une allocation et une copie de moins par variable.

Chercher un nom, c'est calculer son hash, aller dans sa case, et suivre la chaîne :

```c fichier="src/list.c" lignes="48-81"
uint32_t Hash_Name(const char *chaine){
    uint32_t hash = TAB_OFFSET;
    for(; *chaine != '\0'; chaine++){
        hash ^= (unsigned char)*chaine;
        hash *= TAB_PRIME;
    }
    return hash;
}

hash_t *Hash_Get_Hashed(table_t *table, const char *key, uint32_t hash){
    for(hash_t *item = table->TabHash[hash % TAB_SIZE]; item != NULL; item = item->next){
        if(item->hash == hash && (item->key == key || strcmp(item->key,key) == 0))
            return item;
    }
    return NULL;
}

hash_t *Hash_Get(table_t *table, const char *key){
    return Hash_Get_Hashed(table,key,Hash_Name(key));
}

hash_t *Hash_Set(table_t *table, const char *key, void *value){
    uint32_t hash = Hash_Name(key);
    hash_t *item = Hash_Get_Hashed(table,key,hash);
    if(item == NULL){
        item = pool_alloc(&hash_pool,sizeof(*item));
        item->key = key;
        item->hash = hash;
        item->next = table->TabHash[hash % TAB_SIZE];
        table->TabHash[hash % TAB_SIZE] = item;
    }
    item->value = value;
    return item;
}
```

`Hash_Name` est le même FNV-1a que celui des clés de map. Le hash complet est gardé dans la case, comme dans le map, pour comparer deux nombres avant de comparer deux textes. Et si les deux pointeurs `key` sont les mêmes, on n'appelle même pas `strcmp`.

`Hash_Set` met le nouveau nom en tête de sa liste, et réutilise la case si le nom existe déjà : définir deux fois `x` dans la même table remplace la valeur. Les cases viennent de `hash_pool`, un réservoir d'objets de même taille qu'on étudiera au chapitre 8. Pour l'instant, retiens que `pool_alloc` est un `malloc` bien plus rapide.

### Le hash, ligne par ligne

`Hash_Name` mérite qu'on la relise lentement, parce qu'elle est appelée pour chaque nom défini et chaque nom cherché la première fois. Les deux constantes sont en tête de `src/list.c` :

```c fichier="src/list.c" lignes="6-7"
#define TAB_OFFSET 2166136261u
#define TAB_PRIME 16777619u
```

Ce sont les valeurs officielles de **FNV-1a** sur 32 bits, du nom de ses auteurs (Fowler, Noll et Vo). On part de `TAB_OFFSET`, et pour chaque octet du nom, on le mélange au hash avec un ou exclusif, puis on multiplie par `TAB_PRIME`. La multiplication déborde des 32 bits à chaque tour, et c'est voulu : en C, un débordement d'entier *non signé* est défini (on garde le reste modulo 2³²), c'est pour ça que les constantes portent un `u` et que `hash` est un `uint32_t`. Le même calcul sur un `int` serait un comportement indéfini.

Le `(unsigned char)` devant `*chaine` compte aussi. Un `char` est signé sur la plupart des machines, et les octets d'un `é` en UTF-8 (195 et 169) y valent -61 et -87. Converti tel quel en entier, `-61` deviendrait `0xFFFFFFC3`, et le ou exclusif toucherait les 32 bits au lieu des 8 du bas. Le hash resterait cohérent (le même nom donne toujours le même hash), mais moins bien réparti.

FNV-1a n'est pas le meilleur hash du monde, mais il tient en trois lignes, il est rapide sur des textes courts comme des noms de variables, et deux noms qui ne diffèrent que d'une lettre (`v1`, `v2`) tombent loin l'un de l'autre. La répartition de l'essai de la section 6.2 le montre. `hash % TAB_SIZE` choisit la case ; comme `TAB_SIZE` vaut 8, le compilateur le remplace par `hash & 7`.

Rendre une table est l'inverse de `Hash_Set` : chaque case retourne au réservoir.

```c fichier="src/list.c" lignes="83-93"
void Hash_Free(table_t *table){
    for(int i = 0; i < TAB_SIZE; i++){
        hash_t *item = table->TabHash[i];
        while(item != NULL){
            hash_t *next = item->next;
            pool_release(&hash_pool,item);
            item = next;
        }
        table->TabHash[i] = NULL;
    }
}
```

On lit `next` avant de rendre la case : une fois rendue, elle appartient au réservoir, qui écrit dans ses premiers octets le lien vers la case libre suivante (chapitre 8), ou la libère avec `free` dans les compilations de test. La lire ensuite serait lire une mémoire qui n'est plus à nous. Les clés et les valeurs ne sont pas libérées. Les clés appartiennent à l'arbre, les valeurs au garbage collector.

Huit cases, c'est peu. Pour une fonction qui a trois variables locales, c'est parfait : la table tient dans 64 octets et chaque chaîne a au plus un nom. Pour le fichier principal d'un gros script, qui définit des dizaines de fonctions, c'est une autre histoire. On le verra dans un instant avec un programme C.

### Chaîner ou sonder ?

Le map du chapitre 5 range ses clés dans un seul tableau et cherche la case libre suivante en cas de collision : c'est l'**adressage ouvert**. La table des portées accroche à chaque case une liste de tous les noms tombés là : c'est le **chaînage**. Les deux sont classiques. Le chaînage est le plus simple à écrire, il n'a pas besoin de trous pour enlever un nom (une portée n'enlève d'ailleurs jamais rien), et il ne se remplit jamais : une case peut porter autant de noms qu'on veut. Son défaut est qu'il saute de pointeur en pointeur, ce que le processeur aime peu.

Pourquoi une taille fixe, sans jamais grandir ? Parce que la plupart des portées sont minuscules. Une portée par appel de fonction, une par bloc qui déclare un nom, une par tour de `for ... in` : il s'en crée des millions, et presque toutes ont un à cinq noms. Une table qui grandit demanderait une allocation de plus par portée ; huit pointeurs dans la structure ne demandent rien. Le prix est payé par les rares grosses portées, comme celle d'un fichier.

Les autres langages contournent souvent le problème autrement. CPython ne range pas du tout les variables locales d'une fonction dans une table : son compilateur leur donne un numéro, et une variable locale est une case d'un tableau. Seules les globales sont dans un `dict`. C'est exactement ce que fera notre compilateur au chapitre 10, et c'est une des raisons pour lesquelles la VM sera plus rapide.

## 6.2 Une portée et sa parente

Une portée, c'est une de ces tables, plus un pointeur vers la portée qui l'entoure :

```c fichier="src/include/scope.h" lignes="9-17"
    // Une portée : les noms visibles à un endroit du programme, et un lien vers
    // la portée qui l'entoure. Les fonctions, les structures et les variables
    // partagent les mêmes noms.
    typedef struct SCOPE_STRUCT{
        gc_object_t gc;
        table_t variables;
        struct SCOPE_STRUCT *scope_parent;
        list_t *exports;     // noms publiés par `export`, si c'est un fichier
    }scope_t;
```

Tu reconnais l'en-tête `gc` du `value_t`. Une portée est allouée par le garbage collector, comme une valeur, et pour la même raison : on ne sait pas d'avance quand elle cessera de servir. On verra en 6.5 qu'une portée peut survivre très longtemps à la fonction qui l'a créée. `exports` ne sert qu'à la portée d'un fichier, pour `export :: [a, b]` (chapitre 7).

Le fichier `src/scope.c` tout entier fait 38 lignes. Il ajoute à la table une seule idée, celle de la recherche de proche en proche :

```c fichier="src/scope.c" lignes="20-27"
hash_t *scope_lookup_hashed(scope_t *scope, const char *name, uint32_t hash){
    for(; scope != NULL; scope = scope->scope_parent){
        hash_t *item = Hash_Get_Hashed(&scope->variables,name,hash);
        if(item != NULL)
            return item;
    }
    return NULL;
}
```

On cherche ici. Si le nom n'y est pas, on cherche dans la parente, puis dans sa parente, jusqu'à la dernière, dont la parente est `NULL`. Le premier trouvé gagne, ce qui donne le masquage : une variable locale `x` cache la globale `x` sans la toucher.

Pour le programme du début, au moment où `montre` affiche `x`, les portées sont chaînées ainsi (on verra en 6.5 pourquoi celle de `montre` a pour parente le fichier, et pas `appelle`) :

```text
 portée de montre()           portée de appelle()
 (vide)                       x = "locale"
        \                     /
         \ parente           / parente
          v                  v
        portée du fichier
        x = "globale", montre, appelle
                 |
                 | parente
                 v
        portée des builtins
        println, print, input, format, argv...
                 |
                 v
               NULL
```

`scope_lookup` part de la portée de `montre`, ne trouve rien, monte au fichier, y trouve `x`. La portée de `appelle` n'est sur aucun chemin qui part de `montre` : son `x` est invisible.

Il y a donc deux façons de chercher, et le visitor aura besoin des deux. `scope_get` regarde seulement dans cette portée : c'est ce qu'on fait pour savoir si `var x` redéfinit une variable de la même portée. `scope_lookup` remonte la chaîne : c'est ce qu'on fait pour lire `x`. Ce petit programme C les essaie toutes les deux, sur deux portées fabriquées à la main :

```c fichier="livre/essais/chapitres-05-08/ch6/portees.c"
int main(void){
    scope_t *fichier = newScope(NULL);
    scope_t *fonction = newScope(fichier);
    scope_define(fichier, "x", makeString("globale"));
    scope_define(fonction, "y", makeNumber(2));

    printf("scope_get(fonction, \"x\")    : %s\n",
        scope_get(fonction, "x") == NULL ? "NULL" : "trouvé");
    hash_t *x = scope_lookup(fonction, "x");
    printf("scope_lookup(fonction, \"x\") : %s\n", AS_STRING((value_t*)x->value)->str);
    printf("scope_lookup(fichier, \"y\")  : %s\n",
        scope_lookup(fichier, "y") == NULL ? "NULL" : "trouvé");

    // 40 noms dans une seule portée : combien par case ?
    static char noms[40][8];
    for(int i = 0; i < 40; i++){
        snprintf(noms[i], sizeof(noms[i]), "v%d", i);
        scope_define(fichier, noms[i], makeNumber(i));
    }
    for(int i = 0; i < TAB_SIZE; i++){
        int n = 0;
        for(hash_t *item = fichier->variables.TabHash[i]; item != NULL; item = item->next)
            n++;
        printf("case %d : %d noms\n", i, n);
    }
    return 0;
}
```

```terminal
$ ./compile.sh portees
$ ./portees
scope_get(fonction, "x")    : NULL
scope_lookup(fonction, "x") : globale
scope_lookup(fichier, "y")  : NULL
case 0 : 7 noms
case 1 : 4 noms
case 2 : 5 noms
case 3 : 7 noms
case 4 : 4 noms
case 5 : 5 noms
case 6 : 4 noms
case 7 : 5 noms
```

(Le début du fichier inclut `stdio.h`, `Value.h` et `scope.h`. Les noms doivent vivre aussi longtemps que la table, puisqu'elle ne les copie pas : d'où le tableau `static`.)

Les trois premières lignes sont la règle : la fonction voit le fichier, le fichier ne voit pas la fonction. Les huit suivantes sont le prix des huit cases. Quarante et un noms (les quarante `v`, plus `x`) se répartissent bien, mais chaque case en a quatre à sept. Trouver un nom du fichier depuis une fonction, c'est d'abord rater dans la portée de la fonction, puis suivre une chaîne de cinq en moyenne. Garde ça en tête pour le chapitre 9.

Le visitor ajoute une économie au moment de lire une variable :

```c fichier="src/visitor.c" lignes="113-122" focus="115-116"
// Le hash du nom ne se calcule qu'une fois par noeud, et non à chaque lecture.
static hash_t *visitor_find_variable(visitor_t *visitor, ast_t *node){
    if(node->variable_hash == 0)
        node->variable_hash = Hash_Name(node->variable_name);

    hash_t *item = scope_lookup_hashed(visitor->scope,node->variable_name,node->variable_hash);
    if(item == NULL)
        visitor_error(node,"`%s` n'est pas définie",node->variable_name);
    return item;
}
```

Un `x` écrit dans une boucle est lu des millions de fois, mais c'est toujours le même noeud de l'arbre. Son hash est calculé à la première lecture et rangé dans le noeud (`variable_hash`, dans `src/include/ast.h`). C'est la seule chose que le visitor écrit dans l'arbre, et elle ne change pas le sens du programme : c'est un cache. Le chapitre 4 disait que l'arbre ne change jamais pendant l'exécution, et c'est toujours vrai de tout ce qui compte.

Le cache utilise 0 pour dire « pas encore calculé ». Que se passe-t-il si un nom a vraiment 0 pour hash ? Rien de grave : le test `node->variable_hash == 0` reste vrai, et le hash est recalculé à chaque lecture. Le résultat est juste, seulement un peu plus lent pour ce nom-là, une chance sur quatre milliards. Le map du chapitre 5 a fait l'autre choix, en remplaçant un hash nul par 1 (`map_hash_text`). Les deux sont corrects ; le second est plus soigneux.

Si aucune portée ne connaît le nom, le message pointe le noeud :

```ceflo fichier="absente.cf"
func f() {
    var cache = 1
}
f()
println(cache)
```

```terminal
$ ./Ceflo_ref absente.cf
erreur: `cache` n'est pas définie
  --> absente.cf:5:9
    5 | println(cache)
      |         ^
```

## 6.3 Quand un bloc a-t-il sa propre portée ?

Chaque fonction a sa portée, c'est entendu. Mais un `if`, un `while`, un simple bloc `{ }` ? En C, chaque bloc a la sienne. Si on fait pareil, chaque tour de boucle fabrique une portée, même quand le corps ne déclare rien du tout. Une portée, c'est une allocation par le GC, et un tour de boucle doit être rapide.

CeFlo coupe la poire en deux : un bloc a sa propre portée seulement s'il déclare un nom. C'est le parser qui le remarque, en lisant les instructions du bloc une par une :

```c fichier="src/parser.c" lignes="1021-1043" focus="1039"
static bool parser_is_definition(ast_t *node){
    return node->type == AST_VARIABLE_DEFINITION || node->type == AST_FUNCTION_DEFINITION ||
        node->type == AST_STRUCT_DEFINITION || node->type == AST_ENUM_DEFINITION || node->type == AST_INCLUDE;
}

/**
 * @brief les instructions, jusqu'à `}` ou la fin du fichier
 */
static ast_t *parser_parse_statments(parser_t *parser){
    ast_t *block = parser_new_node(parser,AST_COMPOUND);
    block->list_compound = newList();

    while(TYPE(parser) != TOKEN_RBRACKET && TYPE(parser) != TOKEN_EOF){
        if(parser_match(parser,TOKEN_SEMI))
            continue;

        ast_t *child = parser_parse_statment(parser);
        block->has_definition = block->has_definition || parser_is_definition(child);
        List_Append(block->list_compound,child);
    }

    return block;
}
```

`var`, `const`, `func`, `struct`, `enum` et `from ... import` déclarent un nom. Une affectation `x = 2` n'en déclare pas : elle modifie un nom qui existe déjà. Le drapeau `has_definition` reste dans le noeud `AST_COMPOUND`, et le visitor le lit avant d'entrer dans le bloc :

```c fichier="src/visitor.c" lignes="60-70"
// Un bloc { } : il a sa propre portée dès qu'il déclare un nom.
static value_t *visitor_visit_compound(visitor_t *visitor, ast_t *node){
    if(!node->has_definition)
        return visitor_visit_statements(visitor,node);

    scope_t *outer = visitor->scope;
    visitor->scope = newScope(outer);
    visitor_visit_statements(visitor,node);
    visitor->scope = outer;
    return NULL;
}
```

Le visitor garde la portée courante dans `visitor->scope`. Entrer dans un bloc, c'est créer une portée fille de la courante et la rendre courante. En sortir, c'est remettre l'ancienne. L'ancienne portée n'est pas libérée : le GC s'en chargera si plus personne ne la désigne.

Pour l'utilisateur, la différence ne se voit pas, et c'est le but. Un bloc sans déclaration modifie les variables de dehors, ce qu'il ferait de toute façon ; un bloc avec déclaration garde ses noms pour lui :

```ceflo fichier="blocs.cf"
var a = 1
if true {
    a = 2
}
if true {
    var a = 3
    println(a)
}
println(a)
```

```terminal
$ ./Ceflo_ref blocs.cf
3
2
```

Les langages ont fait des choix très différents ici. En C, en Java et en Rust, chaque bloc a sa portée, toujours. Python n'a de portée que pour les fonctions, les classes et les modules : une variable créée dans un `if` existe encore après. JavaScript a les deux : `var` suit la règle de Python, `let` celle du C, et beaucoup de code a dû être réécrit quand `let` est arrivé. CeFlo suit le C pour le sens (un `var` dans un bloc n'en sort pas), et triche seulement pour la mémoire, en ne créant la portée que si quelqu'un y range un nom.

Par contre, le chronomètre la voit. Deux boucles de trois millions de tours, qui ne diffèrent que par une variable dans le corps :

```ceflo fichier="sans_var.cf"
var total = 0
for var i = 0; i < 3000000; i += 1 {
    total += i
}
println(total)
```

```ceflo fichier="avec_var.cf"
var total = 0
for var i = 0; i < 3000000; i += 1 {
    var double = i
    total += double
}
println(total)
```

```terminal
$ time ./Ceflo_ref sans_var.cf
4499998500000

real	0m0,483s
user	0m0,476s
$ time ./Ceflo_ref avec_var.cf
4499998500000

real	0m0,583s
user	0m0,581s
```

Un dixième de seconde de plus, pour une instruction de plus et trois millions de portées. Sans `has_definition`, la première boucle paierait aussi ses trois millions de portées pour rien.

Le `for` lui-même a toujours sa portée, quel que soit son corps, mais une seule pour toute la boucle : c'est là que vit `var i`. On verra son code au chapitre 7 (`visitor_visit_for`). C'est ce qui fait que `i` n'existe plus après la boucle :

```terminal
$ cat apres_for.cf
for var i = 0; i < 3; i += 1 { }
println(i)
$ ./Ceflo_ref apres_for.cf
erreur: `i` n'est pas définie
  --> apres_for.cf:2:9
    2 | println(i)
      |         ^
```

En Python, la variable d'une boucle `for` survit à la boucle et garde sa dernière valeur, ce qui a surpris des générations de débutants (et rendu service à quelques experts). En C depuis C99, comme en Java ou en JavaScript avec `let`, elle disparaît. CeFlo a suivi le C. Le dernier détail est celui des boucles `for ... in`, qui créent une portée neuve à chaque tour, et pas une pour toute la boucle : cette différence a une conséquence étonnante sur les closures, qu'on découvrira au chapitre 7.

Il reste un détail qui surprend la première fois. Une portée se remplit pendant l'exécution, instruction après instruction. Avant que `var x` soit exécuté, le `x` du bloc n'existe pas encore, et c'est celui de dehors qu'on lit :

```ceflo fichier="avant.cf"
var x = 1
if true {
    println(x)
    var x = 2
    println(x)
}
println(x)
```

```terminal
$ ./Ceflo_ref avant.cf
1
2
1
```

Python refuserait ce programme (`UnboundLocalError`), JavaScript aussi avec `let`. CeFlo l'accepte parce que la portée n'est qu'une table, et que la table se lit telle qu'elle est au moment de la lecture. Pour la même raison, une fonction peut en appeler une autre écrite plus bas, tant que l'appel a lieu après que les deux définitions ont été exécutées :

```ceflo fichier="ordre.cf"
func a() { return b() }
func b() { return "b" }
println(a())
println(c())
func c() { return "c" }
```

```terminal
$ ./Ceflo_ref ordre.cf
b
erreur: `c` n'est pas définie
  --> ordre.cf:4:9
    4 | println(c())
      |         ^
```

Quand `a` est définie, `b` n'existe pas encore, et ça ne gêne personne : on ne cherche `b` qu'au moment où `a` s'exécute, ligne 3. À ce moment, la ligne 2 a déjà rangé `b` dans la portée du fichier. `c`, lui, est cherché ligne 4, avant que la ligne 5 l'ait défini.

## 6.4 Les types déclarés et les constantes

Revenons aux deux champs de `hash_t` laissés de côté : `type_name` et `is_const`. Ils viennent de `var age::int` et de `const PI`. Ils sont rangés dans la case de la table, et pas dans la valeur, parce qu'ils appartiennent au nom. `age` est un `int` ; le nombre 22 qu'il contient est juste un nombre, qu'on peut aussi ranger dans une variable libre.

Tout ce qui est faux s'arrête à l'exécution, quand la ligne est atteinte :

```terminal
$ cat types.cf
var a::int = 1
a = 2
println(a)
a = 2.5
$ ./Ceflo_ref types.cf
2
erreur: `a` est de type int, pas float
  --> types.cf:4:1
    4 | a = 2.5
      | ^
```

```terminal
$ cat types.cf
const PI = 3.14
PI = 3
$ ./Ceflo_ref types.cf
erreur: `PI` est une constante
  --> types.cf:2:1
    2 | PI = 3
      | ^
```

La déclaration d'une variable passe par trois étapes : trouver le type, calculer la valeur et la vérifier, puis ranger le nom.

```c fichier="src/visitor.c" lignes="93-111"
static value_t *visitor_visit_variable_definition(visitor_t *visitor, ast_t *node){
    for(; node != NULL; node = node->left){
        const char *type_name = visitor_definition_type(visitor,node);
        value_t *value = NULL;

        if(node->variable_definition_value != NULL){
            value = visitor_visit(visitor,node->variable_definition_value);
            if(type_name != NULL)
                visitor_check_type(visitor,node,node->variable_definition_name,value,type_name);
        }else{
            value = type_name != NULL ? typer_default_value(visitor,node,type_name) : makeNone();
        }

        hash_t *item = scope_define(visitor->scope,node->variable_definition_name,value);
        item->type_name = type_name;
        item->is_const = node->is_const;
    }
    return NULL;
}
```

La boucle sur `node->left` sert à `var a = 1, b = 2` : le parser a enchaîné les deux déclarations par leur champ `left`. `visitor_visit` calcule la valeur, et c'est le premier appel au visitor qu'on voit vraiment : on l'écrira au chapitre 7, et tu peux le lire pour l'instant comme « la valeur de cette expression ».

Trouver le type demande une précaution. Un nom typé le reste, même si on le déclare une deuxième fois dans la même portée :

```c fichier="src/visitor.c" lignes="81-91"
// Une variable typée garde son type, même si on la déclare une deuxième fois.
static const char *visitor_definition_type(visitor_t *visitor, ast_t *node){
    const char *type_name = node->variable_definition_static_type_name;
    hash_t *old = scope_get(visitor->scope,node->variable_definition_name);

    if(old == NULL || old->type_name == NULL)
        return type_name;
    if(type_name != NULL && strcmp(type_name,old->type_name) != 0)
        visitor_error(node,"`%s` est déjà de type %s",node->variable_definition_name,old->type_name);
    return old->type_name;
}
```

C'est `scope_get` et pas `scope_lookup` : seule une déclaration dans la même portée hérite du type. Une variable locale qui porte le nom d'une globale typée est une nouvelle variable, libre de prendre ce qu'elle veut.

```terminal
$ cat types.cf
var a::int = 1
var a = "x"
$ ./Ceflo_ref types.cf
erreur: `a` est de type int, pas str
  --> types.cf:2:5
    2 | var a = "x"
      |     ^
```

La vérification elle-même vit dans `src/typer.c`. C'est une suite de comparaisons de noms :

```c fichier="src/typer.c" lignes="128-154" focus="129-130"
bool typer_has_type(visitor_t *visitor, ast_t *node, value_t *value, const char *type_name){
    if(strcmp(type_name,"int") == 0)
        return typer_is_whole(value);
    if(strcmp(type_name,"float") == 0 || strcmp(type_name,"number") == 0)
        return value->type == NUMBER;
    if(strcmp(type_name,"bool") == 0)
        return value->type == BOOL;
    if(strcmp(type_name,"char") == 0)
        return value->type == CHAR;
    if(strcmp(type_name,"str") == 0 || strcmp(type_name,"string") == 0)
        return value->type == STRING;
    if(strcmp(type_name,"list") == 0)
        return value->type == LIST;
    if(strcmp(type_name,"map") == 0)
        return value->type == MAP;

    value_t *found = typer_find_type(visitor,type_name);
    if(found == NULL)
        visitor_error(node,"le type `%s` n'existe pas",type_name);

    // Un objet peut ne pas encore exister : `var Job::Metier` vaut none.
    if(value->type == NONE)
        return true;
    if(found->type == ENUM)
        return value->type == ENUM_VALUE && value->as.enumItem.owner == found;
    return value->type == OBJECT && value->as.ObjectValue.structure == found;
}
```

Voilà le `int` promis au chapitre 5 : ce n'est pas un type de valeur, c'est la question « ce `number` est-il entier ? » (`typer_is_whole` compare le nombre à `trunc` du nombre). `2.5` est refusé, `2.0` est accepté.

On peut l'essayer sans visitor. Il suffit d'un `visitor_t` avec une portée courante, puisque `typer_has_type` y cherche les structures :

```c fichier="livre/essais/chapitres-05-08/ch6/essai_types.c"
static void essaie(visitor_t *visitor, value_t *value, const char *type){
    printf("%-6s est-il %-5s ? %s\n", AS_STRING(Value_to_string(visitor, value))->str, type,
        typer_has_type(visitor, NULL, value, type) ? "oui" : "non");
}

int main(void){
    ceflo_stack_init();
    visitor_t visitor = {0};
    visitor.scope = newScope(NULL);

    essaie(&visitor, makeNumber(2), "int");
    essaie(&visitor, makeNumber(2.5), "int");
    essaie(&visitor, makeNumber(2.5), "float");
    essaie(&visitor, makeString("2"), "int");
    essaie(&visitor, makeChar('a'), "str");

    value_t *zero = typer_default_value(&visitor, NULL, "int");
    value_t *vide = typer_default_value(&visitor, NULL, "list");
    printf("sans valeur : int -> %s, list -> %s\n",
        AS_STRING(Value_to_string(&visitor, zero))->str, AS_STRING(Value_to_string(&visitor, vide))->str);
    printf("nom dans un message : %s et %s\n", typer_type_name(makeNumber(3)), typer_type_name(makeNumber(3.5)));

    essaie(&visitor, makeNumber(1), "Point");
    return 0;
}
```

```terminal
$ ./compile.sh essai_types
$ ./essai_types; echo "code : $?"
2      est-il int   ? oui
2.5    est-il int   ? non
2.5    est-il float ? oui
2      est-il int   ? non
a      est-il str   ? non
sans valeur : int -> 0, list -> []
nom dans un message : int et float
erreur: le type `Point` n'existe pas
code : 1
```

Lis la quatrième ligne avec attention : c'est la chaîne `"2"`, et elle n'est pas un `int`. `Value_to_string` l'affiche sans guillemets, mais `typer_has_type` regarde `value->type`, pas le texte. Un caractère n'est pas non plus une chaîne d'un octet ici, contrairement à `==` (chapitre 5) : un type déclaré est une promesse stricte. Et un type inconnu arrête tout, avec un message sans position puisque `node` vaut `NULL`.

Si le nom n'est aucun des types de base, c'est peut-être une structure ou une énumération écrite par le script. `typer_find_type` (lignes 92 à 99) la cherche avec `scope_lookup`, comme une variable : `Point` est un nom de la portée, dont la valeur est de type `STRUCT`. Un type est donc soumis aux mêmes règles de visibilité qu'une variable. Une variable de ce type peut valoir `none`, parce qu'un objet n'existe pas toujours dès la déclaration.

```terminal
$ cat types.cf
struct Point { var x = 0 }
var p::Point
println(p)
p = new Point()
println(p.x)
p = 3
$ ./Ceflo_ref types.cf
none
0
erreur: `p` est de type Point, pas int
  --> types.cf:6:1
    6 | p = 3
      | ^
```

Le message dit `int` et pas `number`. C'est `typer_type_name` (ligne 156) qui l'écrit : pour un message, `3` est un `int` et `3.5` un `float`, parce que c'est ce que l'utilisateur comprendra. `var p::Point` sans valeur vaut `none` : c'est `typer_default_value` (lignes 162 à 180) qui choisit la valeur d'une variable typée sans `=`, `0` pour un nombre, `""` pour une chaîne, `[]` pour une liste.

Tu remarques que chaque vérification fait jusqu'à neuf `strcmp` avant de trouver une structure, puis une recherche dans les portées. Une fonction typée `(x::Point) : Point` paie tout ça deux fois à chaque appel. Au chapitre 10, le compilateur transformera le nom du type en un petit numéro, une fois pour toutes.

Les constantes sont plus simples encore. `is_const` est copié de `node->is_const` à la déclaration, et l'affectation le regarde avant d'écrire (on verra `visitor_place_set` au chapitre 7). Une constante est une variable qui refuse `=`.

Mais une constante ne refuse pas `var`. `scope_define`, qu'on a écrite en 6.2, remet `is_const` à `false` chaque fois qu'on range un nom :

```c fichier="src/scope.c" lignes="33-38"
hash_t *scope_define(scope_t *scope, const char *name, struct VALUE_STRUCT *value){
    hash_t *item = Hash_Set(&scope->variables,name,value);
    item->type_name = NULL;
    item->is_const = false;
    return item;
}
```

Ces deux lignes sont nécessaires : `Hash_Set` réutilise la case d'un nom déjà présent, et sans elles une case gardée garderait ses anciens renseignements. Chaque appelant remet ensuite ce qu'il faut. Mais `visitor_visit_variable_definition` ne vérifie que l'ancien type (avec `visitor_definition_type`), pas l'ancienne constante :

```ceflo fichier="const_redef.cf"
const PI = 3.14
var PI = 3
PI = 4
println(PI)
```

```terminal
$ ./Ceflo_ref const_redef.cf
4
```

`var PI` a redéfini la constante en variable ordinaire, et `PI = 4` est passé. `GRAMMAIRE.md` dit qu'un `var x` peut redéfinir une variable de la même portée, mais ne dit rien des constantes ; la machine virtuelle fait la même chose. C'est un coin du langage mal défini, du genre qu'on ne découvre qu'en écrivant un livre dessus. Le défi 4 propose de le fermer.

### Changer de type : @type

`::int` vérifie et ne convertit jamais. Pour convertir, on écrit `@int("42")`. Le parser en fait un noeud `AST_TYPE_CALL`, avec le nom du type dans `function_call_name`, et le visitor (chapitre 7) calcule les arguments puis appelle `typer_typer` :

```c fichier="src/typer.c" lignes="182-208"
value_t *typer_typer(visitor_t *visitor, ast_t *node, value_t **args, int argc){
    const char *type_name = node->function_call_name;

    if(strcmp(type_name,"list") == 0){
        value_t *list = makeList();
        for(int i = 0; i < argc; i++)
            Value_list_append(list,args[i]);
        return list;
    }

    // @Point(1, 2) : une structure se construit comme avec `new`, sauf si elle a un __cast__
    value_t *structure = typer_find_type(visitor,type_name);
    if(structure != NULL && structure->type == STRUCT &&
       (argc != 1 || GetFunctionInStruct(structure->as.structValue.definition,"__cast__") == NULL))
        return visitor_new_object(visitor,node,structure,args,argc);

    if(argc == 0)
        return typer_default_value(visitor,node,type_name);

    Value_check_args(node,type_name,argc,1);

    value_t *result = typer_caste_value(visitor,node,args[0],type_name);
    if(result == NULL)
        visitor_error(node,"impossible de convertir %s en %s",Value_type_name(args[0]),type_name);

    return result;
}
```

Quatre cas, dans l'ordre. `@list(a, b)` fabrique une liste de ses arguments. Une structure sans `__cast__` (ou avec plusieurs arguments) se construit comme avec `new` : c'est pour ça que `typer.c` a besoin de `visitor_new_object`, et donc de notre `provisoire6.c`. Sans argument, on rend la valeur par défaut du type, celle de `var x::int`. Sinon, un seul argument, et `typer_caste_value` (lignes 101 à 122) choisit la conversion selon le nom du type, avec une petite fonction par type cible (`typer_to_int`, `typer_to_char`...). `NULL` veut dire « impossible », et c'est `typer_typer` qui écrit le message.

La conversion la plus surprenante est celle d'une chaîne en nombre :

```c fichier="src/typer.c" lignes="13-24"
static double typer_string_to_number(string_t *str){
    char *end = NULL;
    double number = strtod(str->str,&end);

    while(end != NULL && isspace((unsigned char)*end))
        end++;

    if(str->lenght == 0 || end == str->str || *end != '\0')
        return str->lenght;

    return number;
}
```

`strtod` lit un nombre au début du texte et dit où il s'est arrêté avec `end`. Il saute lui-même les espaces du début ; la boucle saute ceux de la fin. Si le texte est vide, si rien n'a été lu (`end == str->str`), ou s'il reste autre chose que des espaces, la chaîne n'est pas un nombre, et elle donne sa longueur. C'est la règle de `GRAMMAIRE.md`, et c'est un héritage des premières versions de CeFlo, où `@int` d'un texte était sa taille. Python lèverait une exception, JavaScript donnerait `NaN`.

```ceflo fichier="conversions.cf"
println(@int("42") + 1, " ", @int(" 42 "), " ", @int("4a"), " ", @int(3.9))
println(@float("3.5") + 1, " ", @bool(""), " ", @bool([0]), " ", @char(66))
println(@str(12) + "!", " ", @list(1, 2), " ", @int())
println(@map([1]))
```

```terminal
$ ./Ceflo_ref conversions.cf
43 42 2 3
4.5 false true B
12! [ 1 , 2 ] 0
erreur: impossible de convertir list en map
  --> conversions.cf:4:9
    4 | println(@map([1]))
      |         ^
```

`@int("4a")` vaut 2, la longueur de `"4a"`. `@int(3.9)` tronque vers zéro, comme le C. `@bool` suit `Value_is_true` du chapitre 5 : `[0]` n'est pas vide, donc vrai. Et `@int()` sans argument vaut 0, la valeur par défaut.

## 6.5 Les closures : une fonction se souvient d'où elle est écrite

On peut maintenant répondre à la question du début. Quand `montre` s'exécute, dans quelle portée cherche-t-on `x` ? Deux réponses sont possibles.

La première : dans la portée de l'appelant. L'appel crée une portée fille de celle de `appelle`, et `x` y trouve `"locale"`. C'est la **portée dynamique**. Elle est facile à écrire, mais impossible à lire : pour savoir ce que `x` désigne dans `montre`, il faut connaître tous ceux qui l'appellent.

La seconde : dans la portée où `montre` a été écrite. L'appel crée une portée fille de celle du fichier, et `x` y trouve `"globale"`, quel que soit l'appelant. C'est la **portée lexicale**, celle de C, de Python, de JavaScript, et de CeFlo. Il suffit de lire le texte autour de la fonction pour savoir ce que chaque nom désigne.

Pour la portée lexicale, une fonction doit garder la portée où elle a été écrite. C'est exactement ce que contient une valeur `FUNCTION` :

```c fichier="src/include/Value.h" lignes="41-45"
    // Une fonction CeFlo se souvient de la portée où elle a été créée.
    typedef struct{
        ast_t *definition;
        struct SCOPE_STRUCT *closure;
    }Func_t;
```

`definition` est le noeud de l'arbre : les paramètres et le corps, qui ne changent jamais. `closure` est la portée courante au moment où la fonction est créée. Le couple des deux est une **closure**. Créer une fonction, c'est fabriquer ce couple avec la portée courante :

```c fichier="src/visitor.c" lignes="303-310" focus="308"
static value_t *visitor_visit_function_definition(visitor_t *visitor, ast_t *node){
    hash_t *existing = scope_get(visitor->scope,node->function_definition_name);
    if(existing != NULL && ((value_t*)existing->value)->type == FUNCTION)
        visitor_error(node,"la fonction `%s` est déjà définie",node->function_definition_name);

    scope_define(visitor->scope,node->function_definition_name,makeFunction(node,visitor->scope));
    return NULL;
}
```

Une `lfunc`, la fonction sans nom, fait la même chose sans ranger de nom (`case AST_LFUNC: return makeFunction(node,visitor->scope);`, ligne 1033). Et appeler la fonction, c'est créer une portée dont la parente est `closure`, et non la portée de l'appelant :

```c fichier="src/visitor.c" lignes="324-336" focus="326"
    size_t mark = gc_temp_mark();
    scope_t *caller = visitor->scope;
    visitor->scope = newScope(function->as.functionValue.closure);

    for(int i = 0; i < argc; i++){
        ast_t *arg = List_Get(definition->function_definition_args,i);
        if(arg->variable_definition_static_type_name != NULL)
            visitor_check_type(visitor,where,arg->variable_name,args[i],arg->variable_definition_static_type_name);
        hash_t *item = scope_define(visitor->scope,arg->variable_name,args[i]);
        item->type_name = arg->variable_definition_static_type_name;
    }

    visitor_visit_statements(visitor,definition->function_definition_body);
```

Toute la différence entre la portée dynamique et la portée lexicale tient dans la ligne 326. Écris `newScope(caller)` à la place, et `laquelle.cf` affiche `locale`. Les paramètres sont rangés dans la nouvelle portée comme des variables ordinaires, avec leur type s'ils en ont un : un paramètre, c'est une variable déclarée par l'appel. Puis le corps s'exécute dedans. Le reste de la fonction (le `return`, la remise en place de `caller`) attend le chapitre 7.

Le corps est exécuté par `visitor_visit_statements` et pas par `visitor_visit_compound` : la portée de l'appel est déjà neuve, inutile d'en ajouter une deuxième même si le corps déclare des variables.

Voici maintenant ce que la portée lexicale rend possible :

```ceflo fichier="compteur.cf"
func Compteur() {
    var n = 0
    return lfunc() {
        n += 1
        return n
    }
}

var a = Compteur()
var b = Compteur()
println(a(), " ", a(), " ", a())
println(b())
```

```terminal
$ ./Ceflo_ref compteur.cf
1 2 3
1
```

Suis l'exécution avec les portées en tête. `Compteur()` crée une portée, appelons-la P1, qui contient `n = 0`. La `lfunc` est créée pendant que P1 est courante : sa `closure` est P1. `Compteur` retourne, `visitor->scope` revient au fichier, mais P1 n'est pas détruite. La `lfunc` rangée dans `a` la désigne encore.

Chaque `a()` crée une portée fille de P1. `n += 1` y cherche `n`, ne le trouve pas, remonte à P1, l'y trouve et le modifie. Le `n` de P1 vaut 1, puis 2, puis 3. Le deuxième `Compteur()` a créé une autre portée, P2, avec son propre `n` : `b()` part de 0.

```text
 a = <func lfunc>  --closure-->  P1 : n = 3  --parente-->  fichier
 b = <func lfunc>  --closure-->  P2 : n = 1  --parente-->  fichier

 pendant le troisième a() :
   portée de l'appel (vide) --parente--> P1 --parente--> fichier
```

Les portées des appels à `a()` sont vides (la `lfunc` n'a ni paramètre ni `var`), et chacune est oubliée dès que l'appel est fini. P1 et P2, elles, restent : chacune est désignée par la `closure` d'une fonction encore rangée dans une variable.

Il n'y a rien de spécial à écrire pour les closures. Elles tombent toutes seules de deux choix : une portée est un objet alloué sur le tas, et une fonction garde un pointeur vers la sienne. C'est pour ça que le chapitre 12 aura tant de travail : dans la machine virtuelle, une variable locale vivra sur une pile, et disparaîtra au retour de la fonction. Ici, elle vit aussi longtemps que quelqu'un a besoin d'elle.

Le C, lui, n'a pas de closures. Une fonction C est un pointeur vers du code, sans rien autour. Les bibliothèques C qui ont besoin d'en simuler une (`qsort_r`, les fonctions de rappel de GTK) passent à côté du pointeur de fonction un `void *` de « contexte », que le programmeur doit garder en vie lui-même. Notre valeur `FUNCTION` est exactement ce couple, code et contexte, avec le GC pour garder le contexte en vie.

JavaScript se comporte comme CeFlo : chaque appel crée un environnement sur le tas, et une fonction interne le garde. Python aussi, avec une nuance : pour modifier une variable de la fonction qui l'entoure, une fonction interne doit la déclarer `nonlocal`, sinon `n += 1` créerait une nouvelle variable locale. CeFlo n'a pas ce mot : `n += 1` cherche `n` en remontant, comme une lecture, et modifie la case qu'il trouve.

La même mécanique servira une troisième fois, pour les objets. Au chapitre 7, un objet sera une portée qui contient ses attributs, et chacune de ses méthodes sera une fonction dont la `closure` est cette portée. Dans une méthode, `x` trouvera donc l'attribut `x` en remontant d'une portée, exactement comme la `lfunc` du compteur trouve `n`. Les variables, les closures et les objets de CeFlo reposent sur les quarante lignes de `src/scope.c` et la table de `src/list.c`, et sur une seule règle : une fonction garde la portée où elle est née. C'est une économie de moyens qu'on paiera en vitesse, mais qui rend le langage entier facile à expliquer.

Ce « aussi longtemps que quelqu'un a besoin d'elle » est une promesse qu'on n'a pas encore tenue. Qui libère P1 quand `a` n'existe plus ? Pas `Compteur`, qui ne sait pas que sa portée a été capturée. Pas la `lfunc`, qui ne sait pas quand on cesse de l'utiliser. Il faudra quelqu'un qui regarde tout le programme d'en haut, et ce sera le garbage collector du chapitre 8.

## Défis

1. **La portée dynamique.** Dans une copie de `src/visitor.c`, remplace `newScope(function->as.functionValue.closure)` par `newScope(caller)` (ligne 326), recompile avec le reste de `src/`, et lance `laquelle.cf` et `compteur.cf`. Le premier affiche `locale`. Le second s'arrête sur « `n` n'est pas définie » : explique pourquoi avec un dessin des portées, comme celui de la section 6.5.

2. **Un type `natural`.** Ajoute un type qui accepte les nombres entiers positifs ou nuls. Il faut une ligne dans `typer_has_type` et une dans `typer_default_value`. Puis essaie `var n::natural = -1` : le message dit « `n` est de type natural, pas int ». Est-ce clair pour l'utilisateur ? Change `typer_type_name`, ou le message de `visitor_check_type` (`src/visitor.c`, ligne 76), pour dire mieux.

3. **Des tables qui grandissent.** Change la table des portées pour qu'elle double son nombre de cases quand elle a plus de deux noms par case en moyenne (il faudra un pointeur vers un tableau au lieu de `TabHash[TAB_SIZE]`, et tout `src/list.c` à revoir). Mesure `bench/fib.cf` et un script qui définit trois cents fonctions dans un fichier. Lequel des deux y gagne ? Lequel y perd, et pourquoi ?

4. **Des constantes vraiment constantes.** Aujourd'hui, `const PI = 3.14` puis `var PI = 3` est accepté, et `PI` redevient une variable ordinaire (`scope_define` remet `is_const` à `false`). Fais-en une erreur dans `visitor_visit_variable_definition`, sur le modèle de `visitor_definition_type`, qui protège déjà les types. Vérifie que `make test-ref` passe toujours.

## Note de conception

Tout ce chapitre repose sur un choix qu'on fait sans y penser : chercher les noms pendant l'exécution. Chaque lecture de `n` dans `fib` calcule un hash (une seule fois, grâce au cache du noeud), descend dans une case, compare des chaînes, et remonte peut-être trois portées. Chaque appel crée une table. C'est souple : une fonction peut appeler une autre fonction écrite plus bas, une variable peut être définie dans un `if`, `format("${x}")` peut retrouver `x` à partir d'un texte, et rien de tout ça n'a demandé une ligne de plus.

Mais regarde ce qu'on sait déjà en lisant le texte, avant toute exécution. Dans `func fib(n) { ... n ... }`, ce `n` est le paramètre, toujours, pour tous les appels. La portée lexicale garantit qu'on peut le savoir en lisant : c'est même sa définition. On cherche donc des millions de fois, à l'exécution, une réponse qui était écrite dans le texte. Un langage qui a la portée lexicale peut résoudre presque tous ses noms une fois, avant de commencer, et remplacer « cherche `n` » par « prends la case 1 ». C'est ce que font Lua, CPython pour ses variables locales, et notre compilateur au chapitre 10.

Mon avis : la portée lexicale est le bon choix pour tout langage, parce qu'elle rend le code lisible. Et un interpréteur qui a choisi la portée lexicale mais cherche ses noms à l'exécution, comme le nôtre, paie le prix d'une portée dynamique sans en avoir les inconvénients ni les avantages. C'est acceptable pour un premier interpréteur, parce que c'est le plus simple à écrire et à corriger. Mais c'est la première chose qu'on change quand on veut aller vite.

On a des valeurs, et des portées pour les retrouver par leur nom. On a même vu les premières lignes d'un appel de fonction. Mais ces lignes appellent `visitor_visit`, qu'on n'a pas écrit, et sans lui rien ne tourne. Il est temps d'écrire la fonction qui prend un arbre et l'exécute : le visitor. Au bout du prochain chapitre, `fib(30)` affichera 832040.
