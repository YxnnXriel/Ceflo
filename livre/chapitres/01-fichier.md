---
numero: 1
titre: un fichier, une chaîne
note: tout commence par du texte
partie: Lire le texte
---

## 1.1 Ce qu'on va construire

Écris ces cinq lignes dans un fichier `fib.cf` :

```ceflo
func fib(n) {
    if n < 2 { return n }
    return fib(n - 1) + fib(n - 2)
}
println(fib(30))
```

Puis lance-les avec les deux programmes du dépôt :

```terminal
$ time ./Ceflo_ref livre/essais/chapitres-01-04/fib.cf
832040

real	0m0,579s
user	0m0,577s
sys	0m0,001s
$ time ./Ceflo livre/essais/chapitres-01-04/fib.cf
832040

real	0m0,091s
user	0m0,090s
sys	0m0,001s
```

Les deux ont lu le même texte. Les deux ont compris que `fib` est une fonction qui s'appelle elle-même, qu'il faut calculer `n - 1` avant d'appeler, que `<` compare deux nombres et que `return` arrête la fonction. Les deux ont fait les 2 692 537 appels que demande `fib(30)`. L'un a mis six fois plus de temps que l'autre.

Ces deux programmes, c'est CeFlo. Tu vas les écrire. Pas les regarder de loin : les écrire, fichier par fichier, en C, sans aucune bibliothèque à part la bibliothèque standard. À la fin du livre, tu auras un langage de script complet, avec ses fonctions, ses structures, ses listes, ses maps, ses modules, ses closures, et une machine virtuelle qui fait tourner `fib(30)` en neuf centièmes de seconde.

Le livre suit l'ordre dans lequel CeFlo a vraiment été écrit. On commence par l'interpréteur le plus simple qu'on puisse imaginer : il lit le programme, en fait un arbre, puis parcourt cet arbre noeud par noeud pour l'exécuter. C'est `./Ceflo_ref`, qu'on appelle le **visitor**. Il marche, il se lit facilement, et il est lent. Au chapitre 9, on mesurera pourquoi, et on le remplacera par un compilateur et une machine virtuelle : c'est `./Ceflo`.

Le livre a trois parties :

1. **Lire le texte** (chapitres 1 à 4) : transformer un fichier en un arbre qui décrit le programme. C'est commun aux deux interpréteurs.
2. **Exécuter l'arbre** (chapitres 5 à 8) : les valeurs, les portées, le visitor, le garbage collector.
3. **Le bytecode** (chapitres 9 à 14) : les instructions, le compilateur, la machine virtuelle, les closures, les structures, les modules.

On ne te demande pas de savoir comment marche un compilateur. On te demande de savoir écrire du C : un pointeur, un `malloc`, une `struct`, un `switch`. Le reste, on le construit ensemble.

### Comment lire ce livre

Chaque extrait de code vient du dépôt, avec son fichier et ses numéros de ligne. Garde le dépôt ouvert à côté : quand le livre montre `src/lexer.c` lignes 184 à 203, ouvre le fichier à la ligne 184. Tu verras ce qu'il y a autour, et c'est souvent là qu'on comprend le mieux.

Mais le mieux est de ne pas seulement lire. Le dépôt contient le code fini ; ce livre te propose de le réécrire à côté, dans `livre/essais/chapitres-01-04/` (un dossier que git ignore, tu peux y casser ce que tu veux), morceau par morceau, dans l'ordre où on l'écrirait si on partait de rien. Chaque morceau dit où il va. À la fin de chaque section, ce que tu as écrit compile, et un petit programme d'essai montre ce qu'il fait. Quand une fonction du dépôt dépend de quelque chose qui n'existe pas encore, on en écrit d'abord une version plus simple, et on la remplace plus tard. À la fin d'un chapitre, un `diff` entre ton fichier et celui du dépôt ne doit rien afficher.

Lire du code qu'on n'a jamais compilé, c'est comme lire une recette sans jamais cuisiner. On croit avoir compris, jusqu'au jour où on essaie.

Chaque chapitre se termine par des **Défis**, qui t'obligent à modifier le vrai code et disent où regarder pour commencer, et par une **Note de conception**, une page plus libre sur une question que le chapitre a soulevée.

## 1.2 CeFlo en cinq minutes

Avant d'écrire un langage, il faut savoir à quoi il ressemble. Voici un programme qui en montre une bonne partie, rangé dans `livre/essais/chapitres-01-04/tour.cf` :

```ceflo
// Un tour de CeFlo : des notes, une moyenne, un bulletin.
enum Mention { PASSABLE, BIEN, TRES_BIEN }

struct Eleve {
    var Nom
    var Notes::list

    func __init__(nom, notes) {
        Nom = nom
        Notes = notes
    }

    func moyenne() {
        var total = 0
        for var note in Notes {
            total += note
        }
        return total / Notes.lenght()
    }
}
```

On a une énumération, `Mention`, dont les éléments valent 0, 1 et 2. Une structure `Eleve` avec deux attributs : `Nom`, qui peut contenir n'importe quoi, et `Notes`, déclaré avec `::list`, qui ne pourra jamais contenir autre chose qu'une liste. `__init__` est appelé par `new`, et dans une méthode on écrit `Nom` tout court pour parler de l'attribut. Et oui, c'est `lenght`, avec la faute : c'est le nom historique de la méthode dans CeFlo, et `length()` est accepté aussi.

La suite du fichier :

```ceflo
func mention(moyenne) {
    if moyenne >= 16 { return Mention.TRES_BIEN }
    if moyenne >= 14 { return Mention.BIEN }
    return Mention.PASSABLE
}

var classe = [
    new Eleve("Awa", [15, 17, 18]),
    new Eleve("Koffi", [12, 14, 13])
]

var compte = {}
for var eleve in classe {
    var m = mention(eleve.moyenne())
    println(format("${eleve.Nom} : ${eleve.moyenne()} de moyenne, ${m.name}"))
    compte[m] = compte.get(m, 0) + 1
}

match mention(classe[0].moyenne()) {
    case Mention.TRES_BIEN { println("Awa passe en tête") }
    else { println("personne ne passe en tête") }
}
println(compte.lenght(), " mentions différentes")
```

Pas de point-virgule. Pas de parenthèses autour des conditions. Les accolades sont obligatoires. Une liste s'écrit `[...]`, un map `{...}`, et la clé d'un map peut être un élément d'énumération. `format` calcule ce qu'il y a dans chaque `${...}`. `match` prend le premier `case` qui correspond, sans `break` à écrire.

```terminal
$ ./Ceflo livre/essais/chapitres-01-04/tour.cf
Awa : 16.6666666666667 de moyenne, TRES_BIEN
Koffi : 13 de moyenne, PASSABLE
Awa passe en tête
2 mentions différentes
```

`./Ceflo_ref` donne exactement la même chose. C'est même une règle du dépôt : `make diff` lance chaque test avec les deux interpréteurs et vérifie qu'ils sont d'accord. Quand tu écriras la machine virtuelle, l'ancien interpréteur sera ton juge.

> [!attention]
> En écrivant ce programme, j'avais d'abord mis une virgule après le dernier élève, comme on le fait en JavaScript. CeFlo a répondu `` `expression` attendu, mais `]` trouvé ``. Un map accepte une virgule finale, une liste non. Au chapitre 3, tu verras exactement quelle ligne du parser fait cette différence.

Tout le langage est décrit dans `GRAMMAIRE.md`, à la racine du dépôt. Ne le lis pas en entier maintenant : on l'ouvrira au chapitre 3, quand on en aura besoin, et chaque règle deviendra une fonction C.

## 1.3 Le chemin d'un programme

Quand tu tapes `./Ceflo tour.cf`, le programme ne reçoit qu'une chose : une suite d'octets. `v`, `a`, `r`, un espace, `x`... Pour lui, `var` n'est pas un mot, c'est trois lettres. Tout le travail d'un interpréteur consiste à passer de ces octets à une action.

Il le fait en plusieurs étapes, et chacune ne fait qu'une chose :

::: etapes
texte -> jetons -> arbre -> exécution
:::

Le **lexer** découpe le texte en **jetons** : des mots, des nombres, des symboles. Il sait que `var` est un mot réservé, que `12` est un nombre, que `==` est un seul symbole et pas deux `=`. Il ne sait pas ce que veut dire la phrase.

::: jetons "var x = 12, une fois passée dans le lexer"
var TOKEN_VAR
x TOKEN_ID
= TOKEN_EQUAL
12 TOKEN_INTEGER
:::

Le **parser** lit ces jetons et retrouve la structure du programme. Il sait que `var x = 12` est une définition de variable, que `1 + 2 * 3` veut dire `1 + (2 * 3)`, et que le bloc entre accolades appartient au `if` juste avant. Ce qu'il produit, c'est un **arbre** : chaque instruction et chaque calcul devient un noeud, et ses morceaux deviennent ses enfants.

Ensuite seulement, on exécute. Le visitor parcourt l'arbre directement. La machine virtuelle, elle, ajoute deux étapes : un compilateur traduit l'arbre en **bytecode**, une suite d'instructions très simples, et la machine exécute ces instructions. Tu peux déjà les voir :

```terminal
$ cat livre/essais/chapitres-01-04/douze.cf
var x = 12
println(x + 1)
$ ./Ceflo --dis livre/essais/chapitres-01-04/douze.cf
== livre/essais/chapitres-01-04/douze.cf ==
0000     1  CONSTANT              0  12
0003     |  DEFINE_GLOBAL         0
0006     2  GET_BUILTIN           1  "println"
0009     |  GET_GLOBAL            0
0012     |  CONSTANT              2  1
0015     |  ADD
0016     |  CALL                  1
0018     |  POP
0019     1  NONE
0020     |  RETURN

13
```

Pousser 12, le ranger dans `x`, aller chercher `println`, pousser `x`, pousser 1, additionner, appeler. Ne cherche pas à tout comprendre : ce listing, c'est la destination. On y arrivera au chapitre 9. Pour l'instant, retiens que les quatre premiers chapitres, ceux qui transforment le texte en arbre, servent aux deux interpréteurs. On les écrit une fois.

Le `main` de l'ancien interpréteur raconte ce chemin dans son commentaire d'en-tête :

```c fichier="src/main.c" lignes="1-14"
/**
 * @file main.c
 * @author Yann Ariel (yannariel13@gmail.com)
 * @copyright Copyright (c) 2023
 *
 * L'interpréteur suit un parcours simple et basique :
 *      - le lexer découpe le fichier source en jetons
 *      - le parser donne un sens aux jetons et construit l'AST
 *      - le visitor parcourt l'arbre et exécute chaque noeud
 *
 *          [Lexer] ==> [Parser] ==> [AST] ==> [Visitor]
 *
 * Il m'a été demandé par une amie, à qui j'ai dédié son nom.
 */
```

L'AST, c'est l'arbre (*abstract syntax tree*, l'arbre de syntaxe abstraite). Et CeFlo porte le prénom d'une amie : c'est elle qui a demandé ce langage.

Pourquoi découper en étapes ? Parce que chacune devient simple. Le lexer n'a jamais à se demander si `x` est défini. Le parser n'a jamais à compter des espaces. Le visitor ne voit jamais un caractère. Quand un bug apparaît, on sait presque toujours dans quelle étape le chercher. Presque tous les langages sont construits ainsi, de CPython à GCC. Lua est l'exception célèbre : son parser écrit le bytecode au fur et à mesure qu'il lit, sans arbre. C'est plus rapide, et beaucoup plus difficile à lire. On en reparlera au chapitre 4.

Mais avant le lexer, il y a une étape si banale qu'on l'oublie : mettre le fichier en mémoire, et le garder assez longtemps pour pouvoir montrer, le jour d'une erreur, la ligne fautive. C'est l'objet de la fin de ce chapitre, avec les trois ou quatre outils dont tout le reste du code se sert.

## 1.4 Le dépôt et le Makefile

À la racine du dépôt, il y a le `Makefile`, `README.md`, `GRAMMAIRE.md` et quelques dossiers. Le code est dans `src/` :

```terminal
$ ls src
ast.c	    console.c  include	main.c	parser.c  token.c  value_expr.c
bulletin.c  error.c    lexer.c	map.c	scope.c   typer.c  visitor.c
common.c    gc.c       list.c	os.c	str.c	  Value.c  vm
```

Les tests sont dans `tests/` : un fichier `x.cf`, et à côté `x.out`, la sortie qu'il doit donner. `tests/erreurs/` contient 153 petits programmes qui doivent échouer, chacun avec le message attendu sur sa première ligne. `exemple/` montre le langage pas à pas, et `bench/` compare la vitesse de CeFlo à celle de Python.

Dans `src/`, il y a trois groupes de fichiers (les en-têtes sont dans `src/include/`, et la machine virtuelle a son propre dossier, `src/vm/`), et le `Makefile` les nomme dès ses premières lignes :

```makefile fichier="Makefile" lignes="1-12"
CC = gcc
EXEC = Ceflo

# Le lexer, le parser et l'AST : communs aux deux interpréteurs.
FRONT = $(addprefix src/,lexer.c parser.c ast.c token.c error.c str.c list.c common.c)
# La machine virtuelle : le bytecode, le compilateur et les natives.
VM_SRC = $(wildcard src/vm/*.c src/vm/natives/*.c)
# L'ancien interpréteur, qui parcourt l'AST. Il sert de référence pendant la
# réécriture : `make diff` compare ses sorties à celles de la VM.
REF_SRC = $(filter-out $(FRONT),$(wildcard src/*.c))

HEADERS = $(wildcard src/include/*.h src/vm/*.h)
```

`FRONT`, c'est l'avant du langage : huit fichiers, 1 913 lignes de C. C'est tout ce que couvre cette première partie. `REF_SRC` prend tous les autres fichiers de `src/` (le visitor et ses valeurs), avec `filter-out` qui retire ceux de `FRONT`. `VM_SRC` prend tout ce qui est dans `src/vm/`. Avec `wildcard`, un nouveau fichier `.c` est compilé sans qu'on touche au `Makefile`.

Viennent ensuite les options de compilation :

```makefile fichier="Makefile" lignes="13-15"
WARNINGS = -std=c11 -D_GNU_SOURCE -Wall -Wextra
CFLAGS = $(WARNINGS) -O3 -g -flto=auto
LDFLAGS = -rdynamic -lm -ldl
```

Chaque option a sa raison. `-std=c11` fixe la version du C : on aura besoin de `_Noreturn` (C11) et des initialisations nommées comme `{ .line = 1 }` (C99). `-D_GNU_SOURCE` donne accès à quelques fonctions de Linux qui ne sont pas dans la norme. `-Wall -Wextra` allume presque tous les avertissements. `-O3` optimise fort, et `-g` garde quand même de quoi déboguer. `-flto=auto` (*link-time optimization*) laisse gcc optimiser à travers les fichiers : une petite fonction de `common.c` appelée depuis `parser.c` peut être recopiée sur place, comme si elle était dans le même fichier.

Pour l'édition des liens, `-lm` apporte les fonctions mathématiques (`fmod`, `pow`), `-ldl` le chargement de bibliothèques avec `dlopen`, et `-rdynamic` exporte les fonctions de `./Ceflo` pour qu'une bibliothèque `.so` chargée au chapitre 14 puisse les appeler.

La suite du `Makefile` assemble deux programmes avec ces groupes :

```makefile fichier="Makefile" lignes="20-44"
# src/vm passe avant src/include : sur macOS, value.h et Value.h sont le même nom.
VM_INCLUDES = -Isrc/vm -Isrc/include
REF_INCLUDES = -Isrc/include

# Les fichiers .o vont dans build/ : `make BUILD=build/moi` permet à plusieurs
# personnes de compiler en même temps dans le même dossier.
BUILD = build
VM_OBJ = $(patsubst %.c,$(BUILD)/vm/%.o,$(FRONT) $(VM_SRC))
REF_OBJ = $(patsubst %.c,$(BUILD)/ref/%.o,$(FRONT) $(REF_SRC))

all : $(EXEC) $(EXEC)_ref

$(EXEC) : $(VM_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(EXEC)_ref : $(REF_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD)/vm/%.o : %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(VM_INCLUDES) -o $@ -c $<

$(BUILD)/ref/%.o : %.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(REF_INCLUDES) -o $@ -c $<
```

`./Ceflo` est fait de `FRONT` et de `VM_SRC`. `./Ceflo_ref` de `FRONT` et de `REF_SRC`. Les fichiers de `FRONT` sont donc compilés deux fois, une fois dans `build/vm/` et une fois dans `build/ref/`, parce que les deux programmes ne cherchent pas leurs en-têtes dans le même ordre. Le commentaire de la ligne 20 dit pourquoi : macOS ne distingue pas les majuscules dans les noms de fichiers, et `value.h` (la VM) y écraserait `Value.h` (le visitor). Dans les règles, `$@` est la cible, `$<` le premier fichier dont elle dépend, et `$^` tous.

Une autre chose à remarquer : chaque `.o` dépend de *tous* les en-têtes. Toucher un seul `.h` recompile tout. C'est moins fin que les dépendances calculées par `gcc -MMD`, mais c'est impossible à mal configurer, et la compilation complète prend dix secondes.

Lance-la sur un dépôt neuf :

```terminal
$ make
gcc -std=c11 -D_GNU_SOURCE -Wall -Wextra -O3 -g -flto=auto -Isrc/vm -Isrc/include -o build/vm/src/lexer.o -c src/lexer.c
gcc -std=c11 -D_GNU_SOURCE -Wall -Wextra -O3 -g -flto=auto -Isrc/vm -Isrc/include -o build/vm/src/parser.o -c src/parser.c
gcc -std=c11 -D_GNU_SOURCE -Wall -Wextra -O3 -g -flto=auto -Isrc/vm -Isrc/include -o build/vm/src/ast.o -c src/ast.c
gcc -std=c11 -D_GNU_SOURCE -Wall -Wextra -O3 -g -flto=auto -Isrc/vm -Isrc/include -o build/vm/src/token.o -c src/token.c
gcc -std=c11 -D_GNU_SOURCE -Wall -Wextra -O3 -g -flto=auto -Isrc/vm -Isrc/include -o build/vm/src/error.o -c src/error.c
...
gcc -std=c11 -D_GNU_SOURCE -Wall -Wextra -O3 -g -flto=auto -o Ceflo_ref build/ref/src/lexer.o build/ref/src/parser.o ...
```

(On a coupé les 48 lignes de la sortie.) Aucun avertissement, alors que `-Wall -Wextra` est demandé : le dépôt tient à rester propre, et tu devrais t'y tenir aussi. Un avertissement qu'on laisse passer en cache vite dix autres, et le onzième sera un vrai bug.

Puis vérifie que tout marche :

```terminal
$ make test
tests/run.sh ./Ceflo
218 réussis, 0 échoués
```

Dans ce livre, nos programmes d'essai ne passent pas par le `Makefile` : on les compile à la main avec `gcc`, en ne donnant que les fichiers dont ils ont besoin. C'est la meilleure façon de savoir de quoi dépend chaque morceau.

> [!note]
> Le commentaire des lignes 24 et 25 vient d'un vrai besoin : `make BUILD=build/moi` range les `.o` ailleurs. Deux personnes qui compilent dans le même dossier n'écrasent plus les fichiers l'une de l'autre au milieu d'une compilation.

## 1.5 Une mémoire qui ne manque jamais

Le premier fichier qu'on écrit est `src/common.c`, et sa première fonction est celle que tout le reste du code appelle à la place de `malloc`. Commence ton `livre/essais/chapitres-01-04/common.c` par ses `#include` et elle :

```c fichier="src/common.c" lignes="1-25"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
    #include <sys/resource.h>
#endif

#include "include/common.h"

#define STACK_WANTED (64UL * 1024 * 1024)
#define STACK_DEFAULT (1024UL * 1024)
#define STACK_MARGIN (256UL * 1024)

static char *stack_base;
static size_t stack_limit = STACK_DEFAULT;

void *ceflo_alloc(size_t size){
    void *pointer = calloc(1,size > 0 ? size : 1);
    if(pointer == NULL){
        fprintf(stderr,"erreur: mémoire insuffisante\n");
        exit(1);
    }
    return pointer;
}
```

Les lignes 11 à 16 serviront à la fin de la section ; laisse-les de côté pour l'instant. `ceflo_alloc` fait huit lignes, et trois décisions qui changent tout le code qui l'appelle.

La première : si la mémoire manque, le programme s'arrête avec un message. Aucun appelant n'a donc besoin de tester `NULL`. Dans un interpréteur, il y a des centaines d'appels à `ceflo_alloc` ; les tester un par un doublerait la taille du code pour un cas qui n'arrive presque jamais, et chaque test oublié serait un plantage. La plupart des langages font pareil : CPython lève une `MemoryError`, Lua une erreur « not enough memory ». Personne ne continue comme si de rien n'était.

La deuxième : c'est `calloc`, pas `malloc`. La mémoire rendue est remplie de zéros. Retiens-le bien, parce que tout l'avant du langage compte dessus. Un noeud de l'arbre a quarante-six champs, et tous ceux qu'on ne remplit pas valent `NULL`, `0` ou `false` sans qu'on écrive une ligne. Le lexer, dès le chapitre 2, copiera un texte dans un bloc d'un octet de plus, et ne posera jamais le `\0` final : il y est déjà. `calloc` coûte un peu plus cher que `malloc`, puisqu'il écrit des zéros. Sur un bloc fraîchement demandé au système, ces zéros sont souvent gratuits ; et de toute façon, une valeur sûre dans chaque champ vaut bien ce prix.

La troisième est cachée dans `size > 0 ? size : 1`. La norme du C dit qu'un `malloc(0)` ou un `calloc(1, 0)` peut rendre `NULL`, sans que ce soit un manque de mémoire. Sur certains systèmes il le fait. Sans ce test, une liste vide ou une chaîne vide feraient croire à `ceflo_alloc` que la mémoire est épuisée, et le programme s'arrêterait avec un message faux. Demander un octet au lieu de zéro supprime le cas.

La même règle vaut pour l'agrandissement d'un bloc :

```c fichier="src/common.c" lignes="78-90"
void *ceflo_realloc(void *pointer, size_t size){
    pointer = realloc(pointer,size > 0 ? size : 1);
    if(pointer == NULL){
        fprintf(stderr,"erreur: mémoire insuffisante\n");
        exit(1);
    }
    return pointer;
}

void allowChar(char **chaine, const char *value){
    *chaine = ceflo_alloc(strlen(value) + 1);
    strcpy(*chaine,value);
}
```

Écrire `pointer = realloc(pointer, ...)` est une faute classique en C : si `realloc` échoue, il rend `NULL` et on perd l'ancien pointeur, donc l'ancien bloc. Ici, ça n'a pas d'importance, puisqu'on s'arrête aussitôt. `allowChar` copie une chaîne dans un bloc neuf : c'est un `strdup` qui ne rend jamais `NULL`. Son nom est historique ; il reviendra des dizaines de fois.

Entre ces deux fonctions, aux lignes 27 à 76, le fichier contient un réservoir d'objets (`pool_t`) dont seul le visitor se sert. Le chapitre 8 l'expliquera.

### Le garde-fou de la pile

La fin du fichier règle un problème qu'on n'a pas encore, mais qu'on aura au chapitre 3 : un parser et un interpréteur qui s'appellent eux-mêmes, récursivement, pour chaque parenthèse et chaque appel. Une récursion trop profonde fait déborder la pile du C, et le programme meurt d'un *segmentation fault*, sans un mot. Un interpréteur ne doit jamais faire ça, même sur un programme absurde. Il doit dire « trop profondément imbriqué » et s'arrêter proprement.

```c fichier="src/common.c" lignes="95-118" focus="96,115-117"
void ceflo_stack_init(void){
    stack_base = __builtin_frame_address(0);

    #ifndef _WIN32
        struct rlimit limit;
        if(getrlimit(RLIMIT_STACK,&limit) == 0){
            if(limit.rlim_cur != RLIM_INFINITY && limit.rlim_cur < STACK_WANTED &&
                (limit.rlim_max == RLIM_INFINITY || limit.rlim_max >= STACK_WANTED)){
                limit.rlim_cur = STACK_WANTED;
                setrlimit(RLIMIT_STACK,&limit);
            }
            if(getrlimit(RLIMIT_STACK,&limit) == 0){
                stack_limit = limit.rlim_cur == RLIM_INFINITY || limit.rlim_cur > STACK_WANTED ?
                    STACK_WANTED : limit.rlim_cur;
            }
        }
    #endif
}

bool ceflo_stack_exceeded(void){
    char *here = __builtin_frame_address(0);
    size_t used = here < stack_base ? (size_t)(stack_base - here) : (size_t)(here - stack_base);
    return used + STACK_MARGIN > stack_limit;
}
```

`__builtin_frame_address(0)` est une fonction de gcc qui donne l'adresse du cadre de pile de la fonction courante. Appelée tout au début du programme, elle note où commence la pile. Appelée plus tard, au fond d'une récursion, elle dit où on en est. La différence des deux, c'est la place utilisée. Comme la pile descend sur la plupart des machines mais pas sur toutes, la ligne 116 prend la différence dans le bon sens.

Le reste demande au système combien de pile on a droit. Par défaut, Linux en donne 8 Mo au programme principal (`ulimit -s` affiche `8192`). `ceflo_stack_init` essaie d'en obtenir 64 avec `setrlimit`, si la limite maximale le permet, puis relit ce qu'on a vraiment obtenu. `ceflo_stack_exceeded` répond « oui » quand il reste moins de 256 Ko (`STACK_MARGIN`) : de quoi afficher un message d'erreur sans déborder en l'affichant.

Python fait la même chose d'une autre façon : il compte les appels, et s'arrête à 1 000 par défaut (`sys.getrecursionlimit()`). Compter les appels est portable, mais le nombre ne dit rien de la vraie place utilisée, et un appel C très gourmand peut quand même tout faire sauter. Mesurer l'adresse est moins portable (c'est une extension de gcc et de clang), mais exact.

> [!attention]
> `ceflo_stack_init` doit être appelée au début de `main`, avant tout le reste. Sans elle, `stack_base` vaut `NULL`, la différence calculée est immense, et `ceflo_stack_exceeded` répond « oui » dès le premier appel. Le parser refuse alors `var x = 12` avec le message « code trop profondément imbriqué ». On le vérifiera au chapitre 3.

Les déclarations de tout ça sont dans `src/include/common.h`, que tu peux recopier tel quel. Ton fichier compile déjà seul :

```terminal
$ gcc -std=c11 -Wall -Wextra -Isrc -Isrc/include -c livre/essais/chapitres-01-04/common.c -o livre/essais/chapitres-01-04/common.o
```

Le `-Isrc` permet au `#include "include/common.h"` de trouver son fichier depuis `livre/essais/chapitres-01-04/`.

## 1.6 Un tampon qui grandit

On ne connaît presque jamais d'avance la taille d'un texte qu'on construit : un fichier qu'on lit par morceaux, une chaîne dont on traduit les `\n`, un chemin qu'on assemble. Il faut un tampon qui grandit tout seul. Il est déclaré dans `src/include/str.h` :

```c fichier="src/include/str.h" lignes="21-32"
    // Tampon qui grandit tout seul, pour construire les chaînes.
    typedef struct STRUCT_BUFFER{
        char *data;
        size_t size;
        size_t capacity;
    }buffer_t;

    void newBuffer(buffer_t *buffer);
    void Buffer_Append(buffer_t *buffer, const char *data, size_t size);
    void Buffer_Append_Str(buffer_t *buffer, const char *str);
    void Buffer_Append_Char(buffer_t *buffer, char carac);
    void Buffer_Free(buffer_t *buffer);
```

`size` est ce qu'on a écrit, `capacity` ce qu'on a réservé. Un `buffer_t` se déclare sur la pile (`buffer_t buffer;`) et seul son contenu est alloué. Dans ton `livre/essais/chapitres-01-04/str.c`, commence par les `#include` des lignes 1 à 6 du dépôt, puis écris la création et l'ajout :

```c fichier="src/str.c" lignes="53-68" focus="59-63"
void newBuffer(buffer_t *buffer){
    buffer->capacity = 64;
    buffer->size = 0;
    buffer->data = ceflo_alloc(buffer->capacity);
}

void Buffer_Append(buffer_t *buffer, const char *data, size_t size){
    if(buffer->size + size + 1 > buffer->capacity){
        while(buffer->size + size + 1 > buffer->capacity)
            buffer->capacity *= 2;
        buffer->data = ceflo_realloc(buffer->data,buffer->capacity);
    }
    memcpy(buffer->data + buffer->size,data,size);
    buffer->size += size;
    buffer->data[buffer->size] = '\0';
}
```

Lis `Buffer_Append` ligne par ligne. La ligne 60 se demande si la place suffit, en comptant un octet de plus pour le `\0` final (le `+ 1`). Si elle ne suffit pas, la ligne 61 double la capacité *autant de fois qu'il faut* : un `while` et pas un `if`, parce qu'un seul ajout peut être plus grand que tout le tampon (ajouter 1 000 octets à un tampon de 64). La ligne 63 agrandit le bloc une seule fois, à la bonne taille. Puis on copie, on avance `size`, et on pose un `\0` derrière.

Ce `\0` est réécrit à chaque ajout, à un octet plus loin. Le contenu d'un `buffer_t` est donc *toujours* une chaîne C valable, qu'on peut passer à `printf` ou à `strlen` à n'importe quel moment. C'est ce qui permettra au chapitre 2 de lire le texte d'un fichier jusqu'au bout sans jamais tester sa longueur.

Pourquoi doubler, et pas ajouter juste ce qu'il faut ? Parce que `realloc` peut avoir à copier tout le bloc ailleurs. Si on agrandissait d'un octet à chaque ajout, écrire un million d'octets un par un ferait un million de `realloc`, et jusqu'à un million de copies de plus en plus longues : un temps qui grandit comme le carré de la taille. En doublant, on ne fait qu'une quinzaine de `realloc` pour atteindre un mégaoctet, et chaque octet n'est copié, en moyenne, qu'une ou deux fois. C'est la technique de presque tous les tableaux dynamiques : `std::vector` en C++, les listes de Python (qui grandissent d'environ un huitième à la fois), les `slice` de Go.

64 octets au départ, c'est assez pour la plupart des chaînes d'un programme, et assez peu pour qu'on n'ait pas de regret à les gaspiller.

Les trois dernières fonctions sont des raccourcis :

```c fichier="src/str.c" lignes="70-83"
void Buffer_Append_Str(buffer_t *buffer, const char *str){
    Buffer_Append(buffer,str,strlen(str));
}

void Buffer_Append_Char(buffer_t *buffer, char carac){
    Buffer_Append(buffer,&carac,1);
}

void Buffer_Free(buffer_t *buffer){
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
    buffer->capacity = 0;
}
```

`Buffer_Append_Char` passe l'adresse de son paramètre `carac` comme un tableau d'un octet : en C, un `char` dans une variable est un tableau d'un élément. Et `Buffer_Free` remet tout à zéro, pour qu'un tampon libéré ne puisse pas être utilisé par erreur sans planter tout de suite.

### Le voir grandir

Pour croire au doublement, le mieux est de le regarder. Ce petit programme remplit un tampon et affiche chaque changement de capacité :

```c fichier="livre/essais/chapitres-01-04/outils.c"
// Regarde grandir un buffer_t et une list_t.
#include <stdio.h>

#include "str.h"
#include "list.h"

int main(void){
    buffer_t buffer;
    newBuffer(&buffer);
    size_t capacity = buffer.capacity;
    printf("buffer : %zu octets réservés au départ\n",capacity);

    for(int i = 0; i < 1000; i++){
        Buffer_Append_Str(&buffer,"abc");
        if(buffer.capacity != capacity){
            capacity = buffer.capacity;
            printf("  après %4zu octets écrits : %zu réservés\n",buffer.size,capacity);
        }
    }
    Buffer_Free(&buffer);
```

Sa seconde moitié testera la liste de la section 1.8 ; on la verra là-bas, avec la sortie complète. La première partie de la sortie :

```text
buffer : 64 octets réservés au départ
  après   66 octets écrits : 128 réservés
  après  129 octets écrits : 256 réservés
  après  258 octets écrits : 512 réservés
  après  513 octets écrits : 1024 réservés
  après 1026 octets écrits : 2048 réservés
  après 2049 octets écrits : 4096 réservés
```

Trois mille octets écrits, six agrandissements. Le premier arrive à 66 octets et pas à 64 : au 21e ajout, on avait 63 octets, et 63 + 3 + 1 dépasse 64.

## 1.7 Lire un fichier entier

Tout commence par une question bête : comment met-on un fichier dans une chaîne C ?

La réponse qu'on trouve partout : `fseek` à la fin, `ftell` pour connaître la taille, `fseek` au début, un `malloc` de la bonne taille, un seul `fread`. Écrivons-la, pour voir. Elle est dans `livre/essais/chapitres-01-04/lire_naif.c` :

```c fichier="livre/essais/chapitres-01-04/lire_naif.c"
static char *readFile_naif(const char *path){
    FILE *file = fopen(path,"rb");
    if(file == NULL)
        return NULL;

    fseek(file,0,SEEK_END);
    long size = ftell(file);
    fseek(file,0,SEEK_SET);
    if(size < 0){
        fclose(file);
        return NULL;
    }

    char *code = malloc((size_t)size + 1);
    size_t count = fread(code,1,(size_t)size,file);
    code[count] = '\0';
    fclose(file);
    return code;
}
```

Un petit `main` affiche le nombre d'octets lus. Sur un fichier ordinaire, tout va bien :

```terminal
$ gcc -std=c11 -Wall -Wextra -o livre/essais/chapitres-01-04/lire_naif livre/essais/chapitres-01-04/lire_naif.c
$ livre/essais/chapitres-01-04/lire_naif livre/essais/chapitres-01-04/fib.cf
94 octets lus
$ cat livre/essais/chapitres-01-04/fib.cf | livre/essais/chapitres-01-04/lire_naif /dev/stdin
impossible de lire `/dev/stdin`
```

Mais quand le texte arrive par un tube, il n'a pas de taille connue d'avance : on ne peut pas sauter à la fin de quelque chose qui n'est pas encore écrit. `ftell` rend -1, et la lecture échoue. C'est pourtant une façon raisonnable de lancer un script (`générer_le_code | Ceflo /dev/stdin`). CeFlo lit autrement :

```c fichier="src/str.c" lignes="29-51" fonction="readFile" focus="37-40"
char *readFile(const char *path){
    FILE *file = fopen(path,"rb");
    if(file == NULL)
        return NULL;

    buffer_t buffer;
    newBuffer(&buffer);

    char chunk[4096];
    size_t count;
    while((count = fread(chunk,1,sizeof(chunk),file)) > 0)
        Buffer_Append(&buffer,chunk,count);

    bool failed = ferror(file) != 0;
    fclose(file);

    if(failed){
        Buffer_Free(&buffer);
        return NULL;
    }

    return buffer.data;
}
```

On lit par morceaux de 4 096 octets (la taille d'une page mémoire sur la plupart des machines), et on les ajoute au bout du tampon de la section précédente. On ne demande jamais la taille du fichier : on lit jusqu'à ce qu'il n'y ait plus rien. Quand `fread` rend 0, soit le fichier est fini, soit il y a eu une erreur : `ferror` fait la différence. On rend le texte, ou `NULL` si quelque chose a échoué, en libérant ce qu'on avait commencé.

La ligne 50 rend `buffer.data`, le tableau du tampon, et laisse le `buffer_t` lui-même disparaître avec la fonction. C'est l'appelant qui devient propriétaire du texte, et qui devra le libérer. On verra à la section 1.9 qu'il le confie aussitôt à quelqu'un d'autre.

Deux détails comptent. Le `"rb"` d'abord : sous Windows, le mode texte transformerait les `\r\n` en `\n`, et les colonnes ne correspondraient plus aux octets du fichier. En mode binaire, on garde les octets tels quels, et c'est le lexer qui sautera les `\r`. Sous Linux, `"r"` et `"rb"` font la même chose ; écrire `"rb"` coûte un caractère et règle la question partout.

Ensuite, le texte rendu finit par un `\0`. Personne ne l'ajoute ici : c'est `Buffer_Append` qui en pose un à chaque ajout. Et si le fichier est vide ? La boucle ne tourne pas, et on rend le tableau de 64 octets de `newBuffer`, rempli de zéros par `calloc` : une chaîne vide, valable. Aucun cas particulier à écrire.

La fonction ne dit pas *pourquoi* elle a échoué. C'est à l'appelant de décider quoi afficher. La VM, par exemple :

```terminal
$ ./Ceflo pasla.cf
erreur: impossible d'ouvrir `pasla.cf`
```

> [!attention]
> Un fichier qui contient lui-même un octet nul sera coupé à cet endroit : le lexer mesure le texte avec `strlen`. Pour un langage de script, c'est un prix acceptable. Pour un format binaire, ce serait un bug.

Le reste de `src/str.c`, les lignes 8 à 27, définit `String`, une chaîne avec sa longueur rangée dans le même bloc. Seul le visitor s'en sert ; on la retrouvera au chapitre 5. Recopie-les si tu veux que ton fichier soit identique à celui du dépôt.

## 1.8 Une liste de pointeurs

Le parser va fabriquer des listes partout : les instructions d'un bloc, les arguments d'un appel, les éléments d'une liste CeFlo. Il faut une liste de taille variable. La plus simple qui soit, déclarée dans `src/include/list.h` :

```c fichier="src/include/list.h" lignes="6-17"
    // Liste dynamique de pointeurs
    typedef struct LIST_STRUCT{
        void **Item;
        int Size;
        int Capacity;
    }list_t;

    list_t *newList(void);
    void List_Append(list_t *list, void *value);
    void *List_Get(list_t *list, int index);
    void List_Remove(list_t *list, int index);
    void List_Free(list_t *list);
```

Une liste de `void *` : elle peut contenir des pointeurs vers n'importe quoi, des noeuds, des chaînes, des valeurs. C'est l'équivalent C d'une liste générique, au prix d'une conversion à chaque lecture. Le C n'a pas de modèles comme C++ ; on aurait pu écrire une macro qui génère une liste par type (beaucoup de projets C le font), mais pour l'usage de CeFlo, une liste de pointeurs suffit.

Dans ton `livre/essais/chapitres-01-04/list.c` :

```c fichier="src/list.c" lignes="9-20" focus="14-17"
list_t *newList(void){
    return ceflo_alloc(sizeof(list_t));
}

void List_Append(list_t *list, void *value){
    if(list->Size == list->Capacity){
        list->Capacity = list->Capacity == 0 ? 4 : list->Capacity * 2;
        list->Item = ceflo_realloc(list->Item,(size_t)list->Capacity * sizeof(void*));
    }
    list->Item[list->Size] = value;
    list->Size += 1;
}
```

Une liste neuve n'a aucune case : grâce à `calloc`, `Item` vaut `NULL`, `Size` et `Capacity` valent 0, et `realloc(NULL, ...)` se comporte comme `malloc`. La première case n'est réservée qu'au premier ajout, et on en réserve quatre d'un coup. Le parser créera beaucoup de listes qui resteront vides (les arguments de `f()`, les instructions de `{ }`) : elles ne coûteront que leur en-tête.

La lecture et la suppression :

```c fichier="src/list.c" lignes="22-44"
void *List_Get(list_t *list, int index){
    if(index < 0 || index >= list->Size)
        return NULL;
    return list->Item[index];
}

void List_Remove(list_t *list, int index){
    if(index < 0 || index >= list->Size)
        return;
    memmove(
        &list->Item[index],
        &list->Item[index + 1],
        (size_t)(list->Size - index - 1) * sizeof(void*)
    );
    list->Size -= 1;
}

void List_Free(list_t *list){
    if(list == NULL)
        return;
    free(list->Item);
    free(list);
}
```

`List_Get` rend `NULL` pour un index en dehors de la liste, au lieu de lire n'importe où. `List_Remove` décale tous les éléments suivants d'une case vers la gauche avec `memmove` (et pas `memcpy`, qui n'a pas le droit de copier une zone sur une zone qui la chevauche). `List_Free` libère le tableau et l'en-tête, *pas* les objets pointés : la liste ne sait pas ce qu'ils sont, ni s'ils appartiennent à quelqu'un d'autre. Au chapitre 4, on verra que c'est exactement ce qu'il faut pour l'arbre.

Le même fichier contient aussi une table de hachage, `table_t`, aux lignes 46 à 97. On ne s'en sert pas avant le chapitre 6, où elle rangera les variables.

La seconde moitié de `outils.c` regarde grandir une liste :

```c fichier="livre/essais/chapitres-01-04/outils.c"
    list_t *list = newList();
    int list_capacity = list->Capacity;
    printf("liste : %d cases au départ\n",list_capacity);
    for(int i = 0; i < 100; i++){
        List_Append(list,&buffer);
        if(list->Capacity != list_capacity){
            list_capacity = list->Capacity;
            printf("  au %3de élément : %d cases\n",list->Size,list_capacity);
        }
    }
    List_Free(list);
    return 0;
}
```

On range cent fois le même pointeur, peu importe lequel : on ne regarde que la capacité. On compile le programme avec les seuls fichiers dont il a besoin :

```terminal
$ gcc -std=c11 -Wall -Wextra -Isrc/include -o livre/essais/chapitres-01-04/outils livre/essais/chapitres-01-04/outils.c src/str.c src/list.c src/common.c
$ livre/essais/chapitres-01-04/outils
buffer : 64 octets réservés au départ
  après   66 octets écrits : 128 réservés
  après  129 octets écrits : 256 réservés
  après  258 octets écrits : 512 réservés
  après  513 octets écrits : 1024 réservés
  après 1026 octets écrits : 2048 réservés
  après 2049 octets écrits : 4096 réservés
liste : 0 cases au départ
  au   1e élément : 4 cases
  au   5e élément : 8 cases
  au   9e élément : 16 cases
  au  17e élément : 32 cases
  au  33e élément : 64 cases
  au  65e élément : 128 cases
```

Cent éléments, six agrandissements. Remplace `src/str.c src/list.c src/common.c` par tes propres fichiers de `livre/essais/chapitres-01-04/` (avec `-Isrc` en plus) : la sortie doit être la même.

## 1.9 Garder le texte pour les erreurs

Une erreur qui dit seulement `erreur de syntaxe` ne sert à rien. Une erreur qui dit `ligne 3` est mieux. Celle que CeFlo affiche est encore mieux :

```terminal
$ ./Ceflo livre/essais/chapitres-01-04/accents.cf
erreur: `y` n'est pas définie
  --> livre/essais/chapitres-01-04/accents.cf:1:22
    1 | println("déjà vu", y)
      |                    ^
```

Le fichier, la ligne, la colonne, la ligne elle-même, et une flèche sous le coupable. C'est le format qu'a popularisé le compilateur de Rust, et que gcc et clang ont adopté depuis. Pour afficher la ligne fautive, il faut encore avoir le texte du fichier au moment de l'erreur. Or une erreur peut arriver très tard : pendant l'exécution, dans un module importé, longtemps après la lecture. CeFlo garde donc chaque texte lu dans une petite liste chaînée. Commence ton `livre/essais/chapitres-01-04/error.c` :

```c fichier="src/error.c" lignes="1-24" fonction="error_add_source"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "include/error.h"
#include "include/ast.h"

typedef struct SOURCE_STRUCT{
    char *name;
    char *code;
    struct SOURCE_STRUCT *next;
}source_t;

static source_t *sources;

const char *error_add_source(const char *name, char *code){
    source_t *source = ceflo_alloc(sizeof(*source));
    allowChar(&source->name,name);
    source->code = code;
    source->next = sources;
    sources = source;
    return source->name;
}
```

On ajoute chaque source en tête de liste : c'est le plus simple, et l'ordre n'a pas d'importance. Le module des erreurs devient propriétaire du texte (le paramètre `code` n'est pas `const`, c'est le signe qu'on le prend). C'est lui qui le libérera, à la toute fin du programme :

```c fichier="src/error.c" lignes="26-34" fonction="error_free_sources"
void error_free_sources(void){
    while(sources != NULL){
        source_t *next = sources->next;
        free(sources->name);
        free(sources->code);
        free(sources);
        sources = next;
    }
}
```

Personne d'autre ne doit appeler `free` sur un texte confié ici. Et `error_add_source` rend une copie du nom, qu'il garde lui-même. Ce pointeur-là ne bougera plus jusqu'à la fin. Le lexer le recopiera dans chaque jeton, le parser dans chaque noeud de l'arbre. Pour retrouver le texte d'un fichier, il suffit alors de comparer des pointeurs, sans `strcmp` :

```c fichier="src/error.c" lignes="36-51" fonction="error_find_line" focus="38"
static const char *error_find_line(const char *file, int line){
    for(source_t *source = sources; source != NULL; source = source->next){
        if(source->name != file)
            continue;

        const char *cursor = source->code;
        for(int i = 1; i < line; i++){
            cursor = strchr(cursor,'\n');
            if(cursor == NULL)
                return NULL;
            cursor += 1;
        }
        return cursor;
    }
    return NULL;
}
```

Comparer des adresses est plus rapide, mais ce n'est pas la vraie raison. Si un même fichier est lu deux fois sous deux noms (`./a.cf` et `a.cf`), ou si deux sources ont le même nom (toutes les expressions de `format()` s'appellent `"format"`), l'adresse distingue ce que le texte confondrait.

On ne garde pas l'adresse de chaque ligne : on recompte les `\n` depuis le début. C'est lent pour un gros fichier, mais ça n'arrive qu'une fois, juste avant que le programme s'arrête. Les compilateurs qui affichent beaucoup d'erreurs (clang en affiche jusqu'à vingt par défaut) gardent une table des débuts de ligne ; pour CeFlo, qui s'arrête à la première, ce serait de la mémoire pour rien.

L'affichage de la flèche cache un piège :

```c fichier="src/error.c" lignes="56-75" fonction="error_print_location" focus="69-73"
static void error_print_location(const char *file, int line, int colone){
    if(file == NULL)
        return;

    fprintf(stderr,"  --> %s:%d:%d\n",file,line,colone);

    const char *text = error_find_line(file,line);
    if(text == NULL)
        return;

    int lenght = (int)strcspn(text,"\r\n");
    fprintf(stderr,"%5d | %.*s\n",line,lenght,text);
    fprintf(stderr,"      | ");
    for(int i = 0; i < colone - 1 && i < lenght; i++){
        // Les octets de suite UTF-8 n'occupent pas de colonne à l'écran.
        if(((unsigned char)text[i] & 0xC0) != 0x80)
            fputc(text[i] == '\t' ? '\t' : ' ',stderr);
    }
    fprintf(stderr,"^\n");
}
```

`strcspn(text, "\r\n")` donne la longueur de la ligne, jusqu'au premier `\r` ou `\n` : on n'affiche pas la fin de ligne Windows, qui ramènerait le curseur au début. `%.*s` affiche exactement ce nombre de caractères. `%5d` aligne les numéros de ligne sur cinq colonnes, d'où les six espaces de la ligne suivante.

Puis la flèche. La colonne compte des *octets*. Dans `println("déjà vu", y)`, le `y` est au 22e octet, mais seulement au 20e caractère, parce que `é` et `à` prennent deux octets chacun en UTF-8. Si on écrivait 21 espaces, la flèche tomberait deux cases trop à droite. Or en UTF-8, les octets « de suite » d'un caractère commencent tous par les bits `10`, et seuls eux : le test `& 0xC0 == 0x80` les reconnaît, et on ne leur donne pas d'espace. Pour un texte français, ce détail n'a rien d'un luxe. (Il ne règle pas tout : un idéogramme chinois occupe deux colonnes à l'écran, et un emoji parfois plus. Le vrai problème s'appelle la largeur d'affichage, et il demande une table Unicode entière.)

L'autre détail : une tabulation dans la ligne devient une tabulation sous la ligne. Le terminal les aligne de la même façon, et la flèche reste en face. Avec un espace, elle serait décalée de sept colonnes.

Le reste du fichier assemble le tout :

```c fichier="src/error.c" lignes="77-90" focus="83"
static _Noreturn void error_report(const char *title, const char *file, int line, int colone, const char *format, va_list args){
    fflush(stdout);
    fprintf(stderr,"%s: ",title);
    vfprintf(stderr,format,args);
    fprintf(stderr,"\n");
    error_print_location(file,line,colone);
    exit(1);
}

void error_syntax(const char *file, int line, int colone, const char *format, ...){
    va_list args;
    va_start(args,format);
    error_report("erreur de syntaxe",file,line,colone,format,args);
}
```

`error_syntax` reçoit un format et des arguments, comme `printf`, et les passe à `error_report` sous forme de `va_list`, qui les donne à `vfprintf`. Une erreur arrête tout : `exit(1)`, sans retour. C'est pour ça que la fonction est marquée `_Noreturn`, et que le compilateur ne réclame jamais de `return` après un appel à `error_syntax`. Tu verras au chapitre 3 que le parser en profite partout. (Et c'est pour ça qu'on n'appelle jamais `va_end` : on ne revient pas.)

Le `fflush(stdout)` du début s'assure que ce que le programme avait déjà affiché sort avant le message d'erreur, et pas après. `printf` écrit dans un tampon, `stderr` écrit tout de suite : sans ce `fflush`, un programme qui affiche trois lignes puis échoue pourrait montrer l'erreur d'abord, et les trois lignes ensuite.

Les deux dernières fonctions (lignes 92 à 107) font la même chose pour une erreur à l'exécution, en prenant la position dans un noeud de l'arbre. On les lira au chapitre 4.

### Un format vérifié

Les déclarations, dans `src/include/error.h`, ont un attribut de plus :

```c fichier="src/include/error.h" lignes="13-17"
    // Toutes ces fonctions affichent l'erreur et arrêtent le programme.
    _Noreturn void error_syntax(const char *file, int line, int colone, const char *format, ...)
        __attribute__((format(printf,4,5)));
    _Noreturn void visitor_error(const struct AST_STRUCT *node, const char *format, ...)
        __attribute__((format(printf,2,3)));
```

`format(printf,4,5)` dit à gcc : « le quatrième paramètre est un format de `printf`, et les arguments commencent au cinquième ». gcc vérifie alors chaque appel comme il vérifie `printf`. Un essai avec une faute volontaire :

```c fichier="livre/essais/chapitres-01-04/format_faux.c"
// Un appel à error_syntax qui se trompe d'argument : gcc le voit grâce à `format`.
#include "error.h"

int main(void){
    int line = 3;
    error_syntax("essai.cf",line,1,"ligne %s",line);
}
```

```terminal
$ gcc -std=c11 -Wall -Wextra -Isrc/include -c livre/essais/chapitres-01-04/format_faux.c -o livre/essais/chapitres-01-04/format_faux.o
livre/essais/chapitres-01-04/format_faux.c: In function ‘main’:
livre/essais/chapitres-01-04/format_faux.c:6:44: warning: format ‘%s’ expects argument of type ‘char *’, but argument 5 has type ‘int’ [-Wformat=]
    6 |     error_syntax("essai.cf",line,1,"ligne %s",line);
      |                                           ~^  ~~~~
      |                                            |  |
      |                                            |  int
      |                                            char *
      |                                           %d
```

Sans l'attribut, ce programme compilerait sans un mot et planterait le jour de l'erreur, en lisant le nombre 3 comme une adresse. Le pire moment pour planter, c'est pendant qu'on affiche une erreur : on perd le message et la raison. Il y a plus de quatre-vingts appels à ces fonctions dans le dépôt, et gcc les vérifie tous à chaque compilation.

> [!note]
> `colone` s'écrit avec un seul `n` dans tout le code, comme `lenght` s'écrit avec la faute. Ce sont des noms historiques. On les garde tels quels pour que tes recherches dans le dépôt trouvent ce que le livre montre.

## 1.10 Un premier main

On a de quoi écrire un premier programme qui ressemble à un morceau d'interpréteur : il lit un fichier, le confie au module des erreurs, l'affiche avec ses numéros de ligne, et, si on lui donne une ligne et une colonne, affiche une erreur à cet endroit.

```c fichier="livre/essais/chapitres-01-04/lire.c" focus="15,22,33"
// Lit un fichier CeFlo, l'affiche avec ses numéros de ligne,
// puis, si on lui donne une ligne et une colonne, montre une erreur à cet endroit.
#include <stdio.h>
#include <stdlib.h>

#include "str.h"
#include "error.h"

int main(int argc, char **argv){
    if(argc != 2 && argc != 4){
        fprintf(stderr,"utilisation : %s fichier.cf [ligne colonne]\n",argv[0]);
        return 2;
    }

    char *code = readFile(argv[1]);
    if(code == NULL){
        fprintf(stderr,"impossible d'ouvrir `%s`\n",argv[1]);
        return 1;
    }

    // À partir d'ici, le texte appartient au module des erreurs.
    const char *name = error_add_source(argv[1],code);

    int line = 1;
    printf("%5d | ",line);
    for(char *c = code; *c != '\0'; c++){
        putchar(*c);
        if(*c == '\n' && c[1] != '\0')
            printf("%5d | ",++line);
    }

    if(argc == 4)
        error_syntax(name,atoi(argv[2]),atoi(argv[3]),"on regarde ici");

    error_free_sources();
    return 0;
}
```

Remarque qu'on passe `name` à `error_syntax`, le pointeur rendu par `error_add_source`, et pas `argv[1]`. Les deux chaînes contiennent le même texte, mais ce ne sont pas les mêmes adresses : avec `argv[1]`, `error_find_line` ne retrouverait pas le fichier, et la ligne fautive ne s'afficherait pas. Remarque aussi le code de retour : 2 pour une mauvaise utilisation, 1 pour un échec. C'est la convention des outils Unix, et c'est celle des deux `main` du dépôt.

La boucle affiche le numéro de la ligne suivante après chaque `\n`, sauf si c'était le dernier caractère du fichier (`c[1] != '\0'`) : sans ce test, on afficherait un numéro de ligne vide à la fin.

On compile avec tes trois fichiers. `error.c` inclut `ast.h`, mais il ne fait que lire des champs d'une structure : il n'a besoin d'aucune fonction de `ast.c`, et l'éditeur de liens ne réclamera rien.

```terminal
$ gcc -std=c11 -Wall -Wextra -Isrc -Isrc/include -o livre/essais/chapitres-01-04/lire livre/essais/chapitres-01-04/lire.c livre/essais/chapitres-01-04/str.c livre/essais/chapitres-01-04/common.c livre/essais/chapitres-01-04/error.c
$ livre/essais/chapitres-01-04/lire livre/essais/chapitres-01-04/fib.cf 3 12
    1 | func fib(n) {
    2 |     if n < 2 { return n }
    3 |     return fib(n - 1) + fib(n - 2)
    4 | }
    5 | println(fib(30))
erreur de syntaxe: on regarde ici
  --> livre/essais/chapitres-01-04/fib.cf:3:12
    3 |     return fib(n - 1) + fib(n - 2)
      |            ^
```

Avec les accents :

```terminal
$ livre/essais/chapitres-01-04/lire livre/essais/chapitres-01-04/accents.cf 1 22
    1 | println("déjà vu", y)
erreur de syntaxe: on regarde ici
  --> livre/essais/chapitres-01-04/accents.cf:1:22
    1 | println("déjà vu", y)
      |                    ^
```

La flèche tombe sous le `y`, au caractère près. C'est exactement l'affichage de `./Ceflo` pour une vraie erreur. Et le tube qui faisait échouer la version naïve :

```terminal
$ cat livre/essais/chapitres-01-04/fib.cf | livre/essais/chapitres-01-04/lire /dev/stdin
    1 | func fib(n) {
    2 |     if n < 2 { return n }
    3 |     return fib(n - 1) + fib(n - 2)
    4 | }
    5 | println(fib(30))
```

Le vrai CeFlo, qui lit avec la même fonction, peut donc exécuter un programme qui arrive par un tube :

```terminal
$ cat livre/essais/chapitres-01-04/fib.cf | ./Ceflo /dev/stdin
832040
```

Enfin, compare tes fichiers à ceux du dépôt :

```terminal
$ diff livre/essais/chapitres-01-04/common.c src/common.c
$ diff livre/essais/chapitres-01-04/str.c src/str.c
$ diff livre/essais/chapitres-01-04/list.c src/list.c
$ diff livre/essais/chapitres-01-04/error.c src/error.c
```

Rien ne doit s'afficher. Les quatre outils de l'avant du langage sont écrits.

## Défis

1. **Compter.** Fais afficher à `lire` le nombre de lignes et d'octets du fichier à la fin, puis lance-le sur `src/parser.c` : tu sauras combien de lignes il te reste à comprendre. Puis donne-lui une ligne plus grande que le fichier. Que se passe-t-il, et quelle ligne de `error_find_line` l'explique ?

2. **Des tabulations.** Écris un fichier dont une ligne commence par deux tabulations, et demande à `lire` une erreur sur un mot de cette ligne. La flèche tombe-t-elle juste ? Remplace dans `error_print_location` le `'\t'` par un espace, et regarde la flèche se décaler. Puis essaie avec un terminal réglé pour des tabulations de 4 : qu'est-ce qui reste juste, et pourquoi ?

3. **Une table des lignes.** Change `error_add_source` pour qu'elle note, une fois pour toutes, l'adresse du début de chaque ligne dans un `list_t`, et `error_find_line` pour qu'elle s'en serve. Fabrique un fichier d'un million de lignes avec `awk` et mesure, avec `time`, la différence pour une erreur à la dernière ligne. Est-ce que ça valait le coup ?

4. **Grandir moins vite.** Change le facteur de croissance de `Buffer_Append` : multiplie par 1,5 au lieu de 2 (attention, `capacity * 3 / 2` à partir de 1 ne grandit jamais : pourquoi ?). Compte les `realloc` pour écrire 100 Mo octet par octet, dans les deux cas. Les listes de Python grandissent d'environ 12 % : quel avantage y voient-elles ?

## Note de conception : les messages d'erreur font partie du langage

On passe des semaines à choisir la syntaxe d'un langage, et quelques minutes à écrire ses messages d'erreur. C'est à l'envers. Celui qui apprend CeFlo passera plus de temps devant ses erreurs que devant sa grammaire : chaque faute de frappe, chaque accolade oubliée, chaque type qui ne va pas passe par `error_report`. Le message est la partie du langage qu'on lit le plus.

Ce chapitre a montré ce que coûte un bon message : garder tout le texte de chaque fichier jusqu'à la fin du programme, donner une position à chaque jeton et à chaque noeud (seize octets par noeud, on le mesurera au chapitre 4), compter les octets UTF-8 pour placer une flèche. Rien de tout ça ne rend le langage plus rapide ou plus expressif. Tout ça le rend plus facile à vivre.

Elm et Rust ont montré qu'un compilateur pouvait être aimé pour ses messages : ils disent ce qui ne va pas, *où*, et souvent quoi faire. C'est une ambition qu'un petit langage peut se permettre aussi, parce qu'elle ne demande pas de théorie, seulement de l'attention. Dans les chapitres qui viennent, regarde chaque message que CeFlo produit : il nomme ce qui était attendu et ce qui a été trouvé, il pointe la colonne, il propose parfois la correction (« utilisez "..." pour une chaîne »). Chacun de ces messages a été écrit par quelqu'un qui s'est mis à la place de celui qui le lirait.

La règle que je retiens est simple : un message d'erreur doit permettre de corriger le programme sans relancer, sans chercher, et sans ouvrir la documentation. On n'y arrive pas toujours. Mais la question vaut d'être posée à chaque `error_syntax` qu'on écrit.

---

Ce qu'on a écrit lit le texte et sait montrer un endroit précis de ce texte. Mais pour lui, `func fib(n)` n'est encore qu'une suite de treize octets. Il ne sait pas que `func` est un mot réservé, que `fib` est un nom, que `(` ouvre quelque chose qui devra se refermer. Pour qu'un programme puisse dire « `)` attendu » à la bonne colonne, il faut d'abord découper ce texte en morceaux qui ont un sens, et noter pour chacun l'endroit exact où il commence. C'est le travail du lexer, et c'est le chapitre suivant.
