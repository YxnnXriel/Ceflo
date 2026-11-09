---
numero: 5
titre: les valeurs
note: ce que le programme manipule
partie: Exécuter l'arbre
---

Tape ces sept lignes dans un fichier et lance-les avec l'interpréteur qui parcourt l'arbre :

```ceflo fichier="surprises.cf"
println(1 + "a")
println('A' + 1)
println([1, 2] == [1, 2])
println({} == {})
println(0.1 + 0.2)
println(1000000 * 1000000)
println("ab" * 3)
```

```terminal
$ ./Ceflo_ref surprises.cf
1a
66
true
false
0.3
1000000000000
ababab
```

Chaque ligne est une décision. Quelqu'un a décidé qu'un nombre plus une chaîne donne du texte, qu'un caractère plus un nombre donne un nombre, que deux listes se comparent case par case mais que deux maps ne sont égales que si c'est la même. Quelqu'un a décidé que `0.1 + 0.2` s'affiche `0.3` alors que le C, lui, écrirait `0.30000000000000004`.

L'arbre du chapitre 4 ne sait rien de tout ça. Il sait que `1 + "a"` est un `AST_BINOP` avec un `AST_NUMBER` à gauche et un `AST_STRING` à droite. Ce qu'il manque, c'est une chose qu'on puisse calculer, ranger dans une variable, mettre dans une liste et afficher : une **valeur**. On va la construire, de la boîte en C jusqu'au message d'erreur de `[1] - 1`.

### Comment on va travailler

Ce chapitre écrit quatre fichiers : `src/include/Value.h` (la boîte), `src/Value.c` (la fabriquer, la comparer, l'afficher, ses méthodes), `src/map.c` (le map) et `src/value_expr.c` (les opérateurs). Ils n'ont besoin ni du visitor ni du garbage collector pour exister, mais ils les appellent à quelques endroits : `gc_alloc` pour obtenir de la mémoire, `visitor_call` pour la méthode `__str__` d'un objet. Ces deux fonctions arrivent aux chapitres 7 et 8.

En attendant, un petit fichier les remplace par le strict minimum. Il prend la mémoire avec `ceflo_alloc` (le `calloc` du chapitre 1) et ne la rend jamais, et il refuse d'appeler une fonction CeFlo :

```c fichier="livre/essais/chapitres-05-08/ch5/provisoire.c"
// En attendant le chapitre 7 (le visitor) et le chapitre 8 (le GC) : juste de quoi compiler.
#include <stdlib.h>
#include "Value.h"
#include "error.h"

void *gc_alloc(size_t size, gc_kind kind){
    gc_object_t *object = ceflo_alloc(size);
    object->kind = kind;
    return object;
}
void gc_add_bytes(size_t bytes){ (void)bytes; }
void gc_root(void *object){ (void)object; }
void gc_temp_push(void *object){ (void)object; }

// Value.c appelle __str__ d'un objet avec visitor_call : pas d'objets avant le chapitre 7.
struct VALUE_STRUCT *visitor_call(struct VISITOR_STRUCT *visitor, ast_t *node, struct VALUE_STRUCT *function, struct VALUE_STRUCT **args, int argc){
    (void)visitor;(void)function;(void)args;(void)argc;
    visitor_error(node,"pas encore de fonctions CeFlo");
}
```

Et un script compile un petit programme C d'essai avec nos fichiers, ce provisoire, et les outils du chapitre 1 (`str.c`, `list.c`, `common.c`, `error.c`), plus `token.c` pour le nom des opérateurs et `scope.c` dont `Value.c` a besoin pour chercher `__str__` :

```sh fichier="livre/essais/chapitres-05-08/ch5/compile.sh"
#!/bin/sh
# ./compile.sh essai : un programme C avec les valeurs de CeFlo, sans visitor ni GC
SRC=../../../src
gcc -std=c11 -D_GNU_SOURCE -Wall -Wextra -g -I$SRC/include -o $1 $1.c provisoire.c \
    $SRC/Value.c $SRC/value_expr.c $SRC/map.c $SRC/scope.c \
    $SRC/str.c $SRC/list.c $SRC/common.c $SRC/error.c $SRC/token.c -lm
```

Chaque section se termine par un essai compilé ainsi. Les programmes CeFlo du chapitre, eux, sont lancés avec `./Ceflo_ref`, l'interpréteur complet du dépôt : ils montrent ce que nos valeurs feront quand le visitor les fera tourner.

## 5.1 Une boîte pour toutes les valeurs

Une variable CeFlo peut contenir un nombre, puis une chaîne, puis une liste. En C, une variable a un seul type. Il faut donc un type C capable de tout porter, et qui sache dire ce qu'il porte. C'est une étiquette suivie d'une union.

Ouvre un fichier `src/include/Value.h`. Il commence comme tous les en-têtes du dépôt, par une garde contre la double inclusion, puis les en-têtes dont la boîte a besoin :

```c fichier="src/include/Value.h" lignes="1-14"
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
```

Les trois `struct` sans corps sont des déclarations anticipées. Une valeur `FUNCTION` désigne une portée, et une portée contient des valeurs : `Value.h` et `scope.h` auraient besoin l'un de l'autre. Le C ne permet pas d'inclure deux fichiers l'un dans l'autre, mais il permet de dire « il existe une structure de ce nom » et de manipuler des pointeurs vers elle sans connaître son contenu. C'est tout ce dont `Value.h` a besoin.

Puis vient l'étiquette, la liste des types. Elle va juste en dessous, avant la boîte elle-même :

```c fichier="src/include/Value.h" lignes="16-31"
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
```

Quatorze types. Tu reconnais ceux de `GRAMMAIRE.md` : `none`, `bool`, `char`, `number`, `str`, `list`, `map`, les objets. Les autres sont des choses que le script manipule sans les écrire comme des littéraux : une fonction écrite en CeFlo (`FUNCTION`), une fonction écrite en C comme `println` (`BUILTIN`), une structure elle-même (`STRUCT`, ce qu'on obtient en écrivant `Point` sans `new`), une énumération et ses éléments. `GENERIC` est un pointeur C quelconque, pour les bibliothèques natives.

Note qu'il n'y a pas de type entier. CeFlo n'a qu'un `number`, un `double`. `int` existe dans les déclarations (`var a::int`), mais c'est une vérification sur un `number`, pas un autre type de valeur. On le verra au chapitre 6.

Voici la boîte :

```c fichier="src/include/Value.h" lignes="75-93" focus="76-78"
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
```

Trois morceaux. `gc` est l'en-tête dont le garbage collector a besoin pour retrouver et libérer la valeur : on l'ignore jusqu'au chapitre 8. `type` dit lequel des champs de l'union est le bon. `as` contient la donnée elle-même. Une union ne garde qu'un seul de ses champs à la fois : ils partagent la même mémoire, et c'est `type` qui dit lequel lire.

Le nom `as` est choisi pour se lire à voix haute : `value->as.numberValue`, « la valeur, vue comme un nombre ». Comme on l'écrit sans arrêt, des macros le raccourcissent :

```c fichier="src/include/Value.h" lignes="95-100"
    #define AS_BOOL(value) ((value)->as.booleanValue)
    #define AS_CHAR(value) ((value)->as.charValue)
    #define AS_NUMBER(value) ((value)->as.numberValue)
    #define AS_STRING(value) ((value)->as.stringValue)
    #define AS_LIST(value) ((value)->as.listValue)
    #define AS_MAP(value) ((value)->as.mapValue)
```

Aucune de ces macros ne vérifie le type. `AS_NUMBER(v)` sur une chaîne lit les huit octets du pointeur comme si c'était un `double`, et le programme continue avec un nombre absurde. La règle de tout le code qui suit est simple : on regarde `type` avant d'utiliser `AS_`.

Combien pèse cette boîte ? Un petit programme suffit pour le savoir. Il n'a besoin que de l'en-tête :

```c fichier="livre/essais/chapitres-05-08/taille_valeur.c"
#include <stdio.h>
#include "Value.h"

int main(void){
    printf("gc_object_t : %zu octets\n", sizeof(gc_object_t));
    printf("value_t     : %zu octets\n", sizeof(value_t));
    printf("double      : %zu octets\n", sizeof(double));
    return 0;
}
```

```terminal
$ gcc -std=c11 -Wall -Wextra -I../../src/include -o taille_valeur taille_valeur.c
$ ./taille_valeur
gc_object_t : 16 octets
value_t     : 40 octets
double      : 8 octets
```

Quarante octets pour porter un `double` de huit. Le compte est facile à refaire : 16 pour l'en-tête du GC, 4 pour `type` et 4 de remplissage pour aligner la suite, puis 16 pour l'union, dont le plus gros membre (`Obj_t`) tient deux pointeurs. Garde ce chiffre en tête. Au chapitre 9, la machine virtuelle fera tenir une valeur en 16 octets, et ce sera une des raisons de sa vitesse.

### D'autres façons de faire la boîte

Ranger chaque valeur dans un objet alloué, désigné par un pointeur, c'est le choix de CPython : un entier Python est un `PyObject` sur le tas, avec son en-tête, exactement comme notre `value_t`. C'est le plus simple, et c'est pour ça qu'on commence par là.

D'autres langages évitent l'allocation pour les petites valeurs. Lua range ses valeurs dans une structure de 16 octets passée par copie : un nombre y est directement, seuls les chaînes, les tables et les fonctions sont des pointeurs. C'est ce que fera notre VM au chapitre 9. V8 (le moteur JavaScript de Chrome) et OCaml cachent les petits entiers dans le pointeur lui-même : un pointeur est toujours un multiple de 8, ses trois bits du bas sont libres, et un bit à 1 veut dire « ce n'est pas une adresse, c'est un entier ». LuaJIT et JavaScriptCore vont plus loin avec le *NaN-boxing* : un `double` dont l'exposant est plein de 1 (un « NaN ») a 51 bits que personne n'utilise, assez pour y ranger un pointeur. Chaque valeur tient alors en 8 octets.

Toutes ces astuces sont plus rapides, et toutes rendent le code plus difficile à lire et à corriger. Pour un premier interpréteur, la boîte de 40 octets est le bon choix : chaque valeur est un `value_t *`, on peut l'afficher dans un débogueur, et une erreur de type se voit tout de suite.

## 5.2 Fabriquer une valeur

Toutes les valeurs naissent par la même petite fonction :

```c fichier="src/Value.c" lignes="13-17"
static value_t *newValue(type_ceflo type){
    value_t *value = gc_alloc(sizeof(*value),GC_VALUE);
    value->type = type;
    return value;
}
```

`gc_alloc` est le `malloc` du garbage collector. Il rend une zone remplie de zéros, l'enregistre pour pouvoir la libérer plus tard, et c'est tout ce qu'on a besoin de savoir pour l'instant. Au-dessus, une fonction `make` par type remplit le bon champ :

```c fichier="src/Value.c" lignes="45-49"
value_t *makeNumber(double x){
    value_t *value = newValue(NUMBER);
    AS_NUMBER(value) = x;
    return value;
}
```

Lis cette fonction une deuxième fois, parce qu'elle explique beaucoup de choses sur la suite du livre. Chaque nombre calculé est une allocation. `1 + 2` fabrique un `value_t` de 40 octets pour `1`, un autre pour `2`, un troisième pour `3`. Une boucle qui compte jusqu'à dix millions en fabrique des dizaines de millions. Pour l'instant, c'est le prix de la simplicité : tout est un pointeur vers un `value_t`, et le reste du code n'a jamais à se demander si une valeur tient dans un registre ou non.

CPython a le même problème et une astuce partielle : les entiers de -5 à 256 sont fabriqués une fois au démarrage et partagés, comme notre `none`. `a = 7` ne crée rien. CeFlo pourrait faire pareil dans `makeNumber`, avec un tableau de valeurs uniques pour les petits entiers. On ne le fait pas, parce qu'un `double` n'est pas un entier : il faudrait tester `x == floor(x)` et les bornes à chaque appel, et le chapitre 9 réglera le problème à la racine en sortant les nombres du tas.

Trois valeurs échappent à la règle : `none`, `true` et `false`. Elles ne changent jamais et il n'en existe qu'une version de chaque, alors on les fabrique une seule fois :

```c fichier="src/Value.c" lignes="19-37" focus="21-25"
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
```

La variable `static` garde la valeur entre deux appels. Le premier `makeNone()` l'alloue, les suivants la rendent. `gc_root` dit au garbage collector de ne jamais la libérer : sans cette ligne, le GC pourrait jeter `none` pendant qu'un pointeur `static` le désigne encore, et le prochain `makeNone()` rendrait de la mémoire libérée.

Tu as maintenant `newValue`, les fonctions `make` des types simples, et `Value_shared`. C'est assez pour un premier essai. Écris-le dans `livre/essais/chapitres-05-08/ch5/fabrique.c` et compile-le avec le script du début du chapitre :

```c fichier="livre/essais/chapitres-05-08/ch5/fabrique.c"
#include <stdio.h>
#include "Value.h"

int main(void){
    value_t *a = makeNumber(1);
    value_t *b = makeNumber(1);
    printf("makeNumber(1) deux fois : %p et %p\n", (void*)a, (void*)b);
    printf("makeBool(true) deux fois : %p et %p\n", (void*)makeBool(true), (void*)makeBool(true));
    printf("makeNone() deux fois : %p et %p\n", (void*)makeNone(), (void*)makeNone());
    printf("égales ? %s\n", Value_equals(a, b) ? "oui" : "non");
    return 0;
}
```

```terminal
$ ./compile.sh fabrique
$ ./fabrique
makeNumber(1) deux fois : 0x6085767072a0 et 0x6085767072d0
makeBool(true) deux fois : 0x608576708310 et 0x608576708310
makeNone() deux fois : 0x608576708340 et 0x608576708340
égales ? oui
```

Les adresses changent à chaque lancement, mais le motif reste. Deux `1` sont deux objets différents, que `Value_equals` déclare pourtant égaux. Deux `true` sont le même objet, et deux `none` aussi.

Recompile maintenant le même fichier avec les vrais `gc.c` et `visitor.c`, ceux que le `Makefile` a déjà compilés pour `Ceflo_ref` dans `build/ref/src/`. On prend tous ces `.o` sauf `main.o`, et on y ajoute notre `main` :

```terminal
$ OBJ=$(ls ../ref/src/*.o | grep -v main.o)
$ gcc -std=c11 -D_GNU_SOURCE -Wall -Wextra -O3 -flto=auto -I../../src/include -o fabrique fabrique.c $OBJ -rdynamic -lm -ldl
$ ./fabrique
makeNumber(1) deux fois : 0x5e191b552288 et 0x5e191b552260
makeBool(true) deux fois : 0x5e191b552238 et 0x5e191b552238
makeNone() deux fois : 0x5e191b552210 et 0x5e191b552210
égales ? oui
```

Regarde l'écart entre les deux nombres. Avec le provisoire, `0x2d0 - 0x2a0` font 48 octets : les 40 du `value_t`, plus les 8 que `malloc` garde devant chaque bloc pour s'y retrouver. Avec le vrai interpréteur, `0x288 - 0x260` font 40 octets tout juste, et le second nombre est *avant* le premier. Les valeurs sont posées côte à côte dans un grand bloc, en descendant. C'est le réservoir d'objets du chapitre 8, qui évite un `malloc` par nombre ; on y verra pourquoi il distribue ses places à l'envers.

> [!note]
> Les options `-flto=auto` et `-rdynamic` sont celles du `Makefile`. Les `.o` de `build/ref/` sont compilés avec `-flto` : sans cette option au moment de lier, `gcc` ne saurait pas les assembler.

Une chaîne demande un peu plus de travail, parce que son texte vit à part :

```c fichier="src/Value.c" lignes="51-56"
value_t *makeStringLenght(const char *str, int lenght){
    value_t *value = newValue(STRING);
    AS_STRING(value) = new_String_Lenght(str,lenght);
    gc_add_bytes(sizeof(string_t) + (size_t)lenght);
    return value;
}
```

Le nom dit `Lenght` et non `Length`. C'est une faute de frappe d'origine qui s'est répandue dans tout le dépôt, jusqu'à la méthode `lenght()` du langage : on la garde telle quelle. `gc_add_bytes` prévient le GC que la valeur pèse plus que ses 40 octets, pour qu'il sache quand il est temps de faire le ménage.

Le `string_t` lui-même est une petite astuce de `src/str.c` :

```c fichier="src/str.c" lignes="12-20" focus="14-15"
String new_String_Lenght(const char *str, int lenght){
    // L'en-tête et le texte tiennent dans le même bloc.
    String string = ceflo_alloc(sizeof(*string) + (size_t)lenght + 1);
    string->str = (char*)(string + 1);
    memcpy(string->str,str,(size_t)lenght);
    string->str[lenght] = '\0';
    string->lenght = lenght;
    return string;
}
```

Un seul `ceflo_alloc` pour l'en-tête et le texte : `string + 1` pointe juste après la structure, et c'est là que le texte est copié. Un seul `free` suffira à tout libérer. La chaîne garde sa longueur, ce qui permet d'y mettre des `\0` au milieu, et se termine quand même par un `\0` pour qu'on puisse la passer à une fonction C comme `fopen`.

Garder la longueur se voit tout de suite depuis CeFlo :

```terminal
$ cat zero.cf
var s = "a\0b"
println(s.lenght())
$ ./Ceflo_ref zero.cf
3
```

Avec `strlen`, la chaîne aurait fait un octet.

## 5.3 Vrai, faux, égal

Le `if` du chapitre 7 recevra une valeur quelconque et devra choisir. Une seule fonction décide de ce qui compte comme vrai :

```c fichier="src/Value.c" lignes="205-216"
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
```

Ce qui est vide ou nul est faux, le reste est vrai. Un objet, une fonction, une structure sont toujours vrais. La table de `GRAMMAIRE.md` (colonne « Faux quand ») est la traduction exacte de ce `switch`. Vérifions :

```ceflo fichier="verite.cf"
var valeurs = [none, false, 0, "", [], {}, "0", [0], -1]
for var v in valeurs {
    if v {
        println(v, " : vrai")
    } else {
        println(v, " : faux")
    }
}
```

```terminal
$ ./Ceflo_ref verite.cf
none : faux
false : faux
0 : faux
 : faux
[] : faux
{} : faux
0 : vrai
[ 0 ] : vrai
-1 : vrai
```

La quatrième ligne commence par un vide : c'est la chaîne `""`, qui s'affiche sans rien. Et il y a deux lignes `0` qui ne disent pas la même chose. La première est le nombre, la seconde la chaîne `"0"`, qui n'est pas vide, donc vraie. `println` ne met pas de guillemets autour des chaînes, et on verra en 5.4 que ce n'est pas le cas dans une liste.

L'égalité est plus délicate, parce qu'elle mélange les types. La voici en deux morceaux :

```c fichier="src/Value.c" lignes="260-273" focus="261-262"
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
```

D'abord le raccourci : le même objet est égal à lui-même. C'est ce test qui rend deux `none` ou deux `true` égaux sans rien regarder d'autre, puisqu'ils sont uniques.

Ensuite, `bool`, `char` et `number` se comparent comme des nombres. `Value_as_double` donne 1 ou 0 pour un booléen, et le code de l'octet pour un caractère, lu en `unsigned char` pour que `'é'` ne devienne pas négatif. C'est pour ça que `'A' == 65` est vrai.

Puis le texte. `Value_compare_text` accepte une chaîne d'un côté et une chaîne ou un caractère de l'autre, et range un caractère comme une chaîne d'un octet. `'a' == "a"` est donc vrai. Elle sert aussi à `<` et `>`, et c'est pour ça qu'elle rend un ordre (négatif, zéro, positif) et pas juste un booléen.

```c fichier="src/Value.c" lignes="246-258"
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
```

Elle rend deux résultats : l'ordre en valeur de retour, et « c'était bien du texte » par le pointeur `ok`. Le premier test exige au moins une vraie chaîne : deux `char` se comparent comme des nombres, et ont déjà été traités plus haut. `Value_text_view` (lignes 232 à 244) donne un pointeur et une longueur pour une chaîne comme pour un caractère, ce qui permet d'écrire la comparaison une seule fois.

On compare avec `memcmp` et non `strcmp`, parce qu'une chaîne CeFlo peut contenir des `\0` (section 5.2). `memcmp` compare jusqu'à la plus courte des deux longueurs. Si tout est égal jusque-là, la plus courte est la plus petite : `"ab" < "abc"`. C'est l'ordre du dictionnaire, octet par octet, et donc pas tout à fait celui d'un dictionnaire français : `"é"` (deux octets, le premier vaut 195) passe après `"z"` (122).

Il ne reste que des valeurs de même type :

```c fichier="src/Value.c" lignes="275-293"
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
```

Les listes se comparent élément par élément, en s'appelant elles-mêmes pour les listes imbriquées. `ceflo_stack_exceeded` protège cette récursion : une liste imbriquée cent mille fois ferait déborder la pile C, et on préfère un message à un segfault. Tout le reste, maps et objets compris, tombe dans `default` : deux valeurs différentes, même identiques en contenu, ne sont pas égales. Seul le raccourci du début peut les déclarer égales.

```ceflo fichier="egalite.cf"
println(1 == true)
println('A' == 65)
println('a' == "a")
println("1" == 1)
println([1, [2, 3]] == [1, [2, 3]])
var t = { a: 1 }
println(t == { a: 1 })
println(t == t)
println(none == none)
println("abc" < "abd", " ", "b" > "abc")
```

```terminal
$ ./Ceflo_ref egalite.cf
true
true
true
false
true
false
true
true
true true
```

`"1" == 1` est faux : une chaîne n'est jamais un nombre, même si elle en a l'air. Il faut écrire `@int("1")` pour convertir, et c'est voulu.

## 5.4 Le texte d'une valeur

`println`, la concaténation `"x=" + x`, `@str(x)` et les messages d'erreur ont tous besoin du texte d'une valeur. Une seule fonction l'écrit, dans un `buffer_t`, le tampon qui grandit tout seul du chapitre 1 :

```c fichier="src/Value.c" lignes="416-440"
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
```

Le reste du `switch` (lignes 441 à 469) écrit `<func println>`, `<struct Point>`, `Couleur.ROUGE` : des étiquettes, sans surprise. Trois choses méritent qu'on s'arrête.

La première, ce sont les nombres. Un `double` affiché avec `%g` donne `1e+06` pour un million, ce qu'aucun utilisateur ne veut voir. `Value_number_to_buffer` traite donc à part le cas d'un nombre entier :

```c fichier="src/Value.c" lignes="325-346" focus="330"
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
```

Les chiffres s'écrivent à l'envers, de la fin du tableau vers le début, parce que `% 10` donne d'abord le chiffre des unités. C'est plus rapide que `snprintf`, et ça compte : afficher des nombres est ce que font la plupart des scripts. Au-delà de 10¹⁵, ou pour un nombre à virgule, on passe par `%.15g`. Quinze chiffres significatifs, pas dix-sept : c'est ce qui fait afficher `0.3` pour `0.1 + 0.2`, en arrondissant l'erreur de calcul du `double` avant qu'elle n'apparaisse.

La deuxième, ce sont les éléments d'une liste ou d'un map. Afficher `[1, "1"]` comme `[ 1 , 1 ]` mentirait. `Value_item_to_buffer` remet donc des guillemets autour des chaînes et des apostrophes autour des caractères, mais seulement à l'intérieur d'une liste ou d'un map :

```c fichier="src/Value.c" lignes="361-374"
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
```

La troisième, c'est le drapeau `printing`, rangé dans l'en-tête du GC. Une liste peut se contenir elle-même : `l.append(l)`. Sans précaution, l'afficher serait une récursion infinie. Avant d'écrire une liste, un map ou un objet, on lève son drapeau. Si on retombe dessus pendant qu'il est levé, on écrit `...` au lieu de recommencer.

```ceflo fichier="affichage.cf"
println(1, " ", "1", " ", 'a')
println([1, "1", 'a', none])
println({ nom: "Yann", age: 22 })
println(2.5, " ", 1 / 3, " ", 1000000 * 1000000 * 10000)
var l = [1, 2]
l.append(l)
println(l)
println(println)
```

```terminal
$ ./Ceflo_ref affichage.cf
1 1 a
[ 1 , "1" , 'a' , none ]
{ "nom" : "Yann" , "age" : 22 }
2.5 0.333333333333333 1e+16
[ 1 , 2 , ... ]
<func println>
```

Pourquoi `Value_to_buffer` reçoit-il le `visitor` ? À cause des objets. Une structure peut définir `__str__`, et l'afficher veut dire appeler cette méthode CeFlo, donc exécuter du code. `Value_object_to_buffer` (lignes 348 à 359) cherche `__str__` dans l'objet et l'appelle avec `visitor_call`, qu'on écrira au chapitre 7. Afficher une valeur peut donc lancer n'importe quel calcul.

Sans visitor, on peut déjà tout afficher sauf les objets. Un essai en C, avec une liste qui se contient elle-même :

```c fichier="livre/essais/chapitres-05-08/ch5/essai_texte.c"
#include <stdio.h>
#include "Value.h"

int main(void){
    ceflo_stack_init();
    value_t *l = makeList();
    Value_list_append(l, makeNumber(1));
    Value_list_append(l, makeString("1"));
    Value_list_append(l, makeChar('a'));
    Value_list_append(l, makeNumber(0.1 + 0.2));
    Value_list_append(l, l);

    Value_print(NULL, stdout, l);
    printf("\n");
    Value_print(NULL, stdout, makeNumber(1e15));
    printf(" ");
    Value_print(NULL, stdout, makeNumber(123456789012345));
    printf("\n");
    return 0;
}
```

```terminal
$ ./compile.sh essai_texte
$ ./essai_texte
[ 1 , "1" , 'a' , 0.3 , ... ]
1e+15 123456789012345
```

Le visitor vaut `NULL`, et c'est permis tant qu'aucun objet n'est affiché. `1e15` est juste à la limite : `fabs(number) < 1e15` est faux, on passe par `%.15g`. Un chiffre de moins, et on reste dans le chemin rapide.

> [!attention]
> La première version de cet essai n'avait pas la ligne `ceflo_stack_init()`, et elle affichait « erreur: valeur trop profonde pour être affichée » dès la première liste. `ceflo_stack_exceeded` mesure la distance entre l'adresse courante de la pile et celle notée par `ceflo_stack_init`. Sans elle, l'adresse de départ vaut `NULL`, et la pile semble profonde de plusieurs téraoctets. Le vrai `main` (chapitre 7) l'appelle en premier.

Au-dessus de `Value_to_buffer`, deux fonctions d'une dizaine de lignes : `Value_print` écrit le texte dans un `FILE *`, et `Value_to_string` en fait une valeur `str`. C'est tout ce dont `println` aura besoin.

## 5.5 Les chaînes ne changent jamais

Regarde ce programme et devine ce qu'il affiche avant de lire la sortie :

```ceflo fichier="chaines.cf"
var a = "chat"
var b = a
a[0] = 'p'
println(a, " ", b)

var l = [1, 2]
var m = l
l[0] = 9
println(l, " ", m)
println("é".lenght())
```

```terminal
$ ./Ceflo_ref chaines.cf
phat chat
[ 9 , 2 ] [ 9 , 2 ]
2
```

Les deux moitiés ont l'air d'être le même programme, et pourtant elles ne font pas la même chose. Après `var b = a`, les deux variables désignent le même `value_t`, que ce soit une chaîne ou une liste : rien n'a été copié. Modifier la liste par `l` la modifie donc pour `m` aussi. Pour la chaîne, `a[0] = 'p'` ne touche pas au texte de `"chat"`. Il fabrique une nouvelle chaîne `"phat"` et la range dans `a`. `b` désigne toujours l'ancienne.

On verra le code de `a[0] = 'p'` au chapitre 7 (`visitor_place_set_char`). Ce qui compte ici, c'est la règle : un `string_t` ne change jamais après sa création. Cette règle rapporte trois choses.

On peut partager une chaîne sans danger. `var b = a` n'a pas besoin de copier le texte, et passer une chaîne de dix mégaoctets à une fonction coûte un pointeur. Le commentaire en tête de `src/visitor.c` le dit : les nombres, les caractères et les chaînes ne changent jamais, `a = b` peut donc les partager.

On peut garder des calculs faits une fois. Regarde le champ `hash` de `string_t` :

```c fichier="src/include/str.h" lignes="7-11"
    typedef struct STRUCT_STR{
        char *str;
        int lenght;
        uint32_t hash;   // calculé au premier besoin par map.c, 0 tant qu'il ne l'est pas
    }string_t, *String;
```

Une chaîne utilisée comme clé de map n'a son hash calculé qu'une fois. Si le texte pouvait changer, le hash gardé deviendrait faux, et la chaîne serait perdue dans la table.

Et le comportement reste celui que l'utilisateur attend. En Python, en JavaScript ou en Java, une chaîne ne se modifie pas en place, et ceux qui viennent de ces langages s'attendent à ce que `b` reste `"chat"`.

La dernière ligne rappelle qu'une chaîne compte en octets. `"é"` s'écrit en UTF-8 sur deux octets, donc sa longueur vaut 2. C'est un choix de simplicité : `s[i]` est un accès direct au tableau, en temps constant.

## 5.6 Les listes, et les maps qui gardent l'ordre

La liste CeFlo n'a rien à elle. Sa donnée est un `list_t`, la liste de pointeurs du chapitre 1, avec `Item`, `Size` et `Capacity`, et `List_Append` qui double la capacité quand elle est pleine. Une liste de valeurs est une liste de `value_t *`. La seule fonction propre à `Value.c` prévient le GC que la liste grossit :

```c fichier="src/Value.c" lignes="136-139"
void Value_list_append(value_t *list, value_t *item){
    List_Append(AS_LIST(list),item);
    gc_add_bytes(sizeof(void*));
}
```

Le map demande plus de travail, parce qu'on lui demande deux choses qui se contredisent. Il doit trouver une clé en temps constant, quelle que soit sa taille : c'est une table de hachage. Et il doit rendre ses clés dans l'ordre où on les a ajoutées, comme le `Map` de TypeScript : une table de hachage classique les rend dans le désordre des cases. La solution tient dans deux tableaux, décrits en tête de `src/include/map.h` :

```c fichier="src/include/map.h" lignes="22-34"
    typedef struct{
        struct VALUE_STRUCT *key;     // NULL : la paire a été enlevée
        struct VALUE_STRUCT *value;
        uint32_t hash;
    }map_entry_t;

    typedef struct{
        map_entry_t *entries;
        int32_t *index;
        int size;       // les cases utilisées dans entries, trous compris
        int count;      // les clés vivantes
        int capacity;
    }map_t;
```

`entries` range les paires dans l'ordre d'arrivée, l'une après l'autre. Le parcourir de 0 à `size` donne l'ordre d'insertion. `index` est la table de hachage, mais elle ne contient que des numéros : pour un hash, la position de la paire dans `entries`, ou `-1` pour une case vide. `index` a deux fois plus de cases que `entries`, donc il est toujours au plus à moitié plein.

Le plus simple est de regarder dedans. Ce programme C crée un map, y met trois clés, en enlève une, en ajoute d'autres, et affiche les deux tableaux à chaque étape (la fonction `montre` parcourt `entries` et `index` ; le fichier complet est dans `livre/essais/chapitres-05-08/ch5/dans_le_map.c`) :

```c fichier="livre/essais/chapitres-05-08/ch5/dans_le_map.c"
int main(void){
    value_t *t = makeMap();
    Value_map_set_name(t, "nom", makeString("Yann"));
    Value_map_set_name(t, "age", makeNumber(22));
    Value_map_set_name(t, "ville", makeString("Abidjan"));
    montre(AS_MAP(t));

    map_remove(AS_MAP(t), makeString("age"));
    montre(AS_MAP(t));

    Value_map_set_name(t, "pays", makeString("CI"));
    montre(AS_MAP(t));

    const char *noms[] = {"a", "b", "c", "d", "e"};
    for(int i = 0; i < 5; i++)
        Value_map_set_name(t, noms[i], makeNumber(i));
    printf("après a..e : size=%d count=%d capacity=%d\n", AS_MAP(t)->size, AS_MAP(t)->count, AS_MAP(t)->capacity);

    Value_map_set_name(t, "f", makeNumber(5));
    montre(AS_MAP(t));
    return 0;
}
```

```terminal
$ ./compile.sh dans_le_map
$ ./dans_le_map
size=3 count=3 capacity=8
  entries : [0] nom [1] age [2] ville
  index   : . . 2 . . . . . . . . . 0 . . 1
size=3 count=2 capacity=8
  entries : [0] nom [1] (trou) [2] ville
  index   : . . 2 . . . . . . . . . 0 . . 1
size=4 count=3 capacity=8
  entries : [0] nom [1] (trou) [2] ville [3] pays
  index   : . . 2 . . . . 3 . . . . 0 . . 1
après a..e : size=8 count=8 capacity=16
size=9 count=9 capacity=16
  entries : [0] nom [1] ville [2] pays [3] a [4] b [5] c [6] d [7] e [8] f
  index   : 8 . 1 . . . . . . . . . . . 6 . . 5 4 3 7 . . 2 . . . . 0 . . .
```

Suis la clé `nom`. Son hash, réduit aux 16 cases de `index`, tombe sur la case 12. La case 12 contient `0` : la paire est `entries[0]`. `age` tombe en 15, `ville` en 2. Chercher une clé, c'est calculer son hash, sauter à sa case, et lire le numéro :

```c fichier="src/map.c" lignes="81-92"
static int map_find_text(map_t *map, uint32_t hash, const char *text, int lenght){
    if(map->index == NULL)
        return -1;

    uint32_t mask = (uint32_t)map->capacity * 2 - 1;
    for(uint32_t i = hash & mask; map->index[i] != MAP_EMPTY; i = (i + 1) & mask){
        map_entry_t *entry = &map->entries[map->index[i]];
        if(entry->hash == hash && entry->key != NULL && map_is_text(entry->key,text,lenght))
            return map->index[i];
    }
    return -1;
}
```

La taille de `index` est toujours une puissance de deux, donc `hash & mask` est un modulo rapide. Si la case est prise par une autre clé, on essaie la suivante, jusqu'à une case vide qui veut dire « absent ». On compare d'abord les hash, qui sont gardés dans la paire : deux nombres de 32 bits se comparent bien plus vite que deux textes, et on ne fait `memcmp` que s'ils sont égaux.

Maintenant, regarde la deuxième étape. Enlever `age` n'a pas touché à `index` : la case 15 contient toujours `1`, qui désigne maintenant un trou. C'est voulu, et c'est le piège classique des tables de hachage à sondage linéaire.

> [!attention]
> Si `map_remove` remettait la case de `index` à `-1`, il couperait les chaînes de recherche. Une clé qui avait trouvé la case 15 prise et s'était rangée en 0 (la case suivante, en faisant le tour) deviendrait introuvable : sa recherche s'arrêterait sur la case 15, vide. On laisse donc la case pointer vers un trou, et la recherche passe par-dessus (`entry->key != NULL`).

```c fichier="src/map.c" lignes="210-220"
value_t *map_remove(map_t *map, value_t *key){
    int position = map_find(map,key,map_hash_key(key));
    if(position < 0)
        return NULL;

    value_t *value = map->entries[position].value;
    map->entries[position].key = NULL;
    map->entries[position].value = NULL;
    map->count -= 1;
    return value;
}
```

D'où les deux compteurs : `size` compte les cases utilisées dans `entries`, trous compris, et `count` les clés vivantes. `t.lenght()` rend `count`.

Les trous ne restent pas pour toujours. Quand `entries` est plein, `map_set` appelle `map_rebuild` (lignes 153 à 182), qui recopie les paires vivantes dans un tableau neuf, sans les trous, et reconstruit `index`. Dans la sortie, c'est arrivé à l'ajout de `e` : `size` valait 8, autant que `capacity`. Le trou de `age` a disparu, et la capacité est passée à 16 pour garder de la place. Les clés sont restées dans l'ordre : `nom`, `ville`, `pays`, `a`...

```c fichier="src/map.c" lignes="153-182"
static void map_rebuild(map_t *map){
    size_t before = map_bytes(map);

    int capacity = MAP_FIRST_CAPACITY;
    while(capacity < map->count * 2)
        capacity *= 2;

    map_entry_t *entries = ceflo_alloc((size_t)capacity * sizeof(map_entry_t));
    int size = 0;
    for(int i = 0; i < map->size; i++){
        if(map->entries[i].key != NULL){
            entries[size] = map->entries[i];
            size += 1;
        }
    }
    free(map->entries);
    free(map->index);

    map->entries = entries;
    map->size = size;
    map->capacity = capacity;
    map->index = ceflo_alloc((size_t)capacity * 2 * sizeof(int32_t));
    memset(map->index,0xFF,(size_t)capacity * 2 * sizeof(int32_t));

    for(int i = 0; i < size; i++)
        map_index_add(map,entries[i].hash,i);

    if(map_bytes(map) > before)
        gc_add_bytes(map_bytes(map) - before);
}
```

La capacité part de 8 et double jusqu'à valoir au moins deux fois le nombre de clés vivantes. Elle suit donc les clés, pas les trous : un map dont on a enlevé presque tout rétrécit à la reconstruction suivante. La copie garde l'ordre, puisqu'elle parcourt l'ancien `entries` du début à la fin. Le `memset` à `0xFF` met chaque `int32_t` de `index` à -1, c'est-à-dire `MAP_EMPTY` : quatre octets à `0xFF`, c'est -1 en complément à deux. Puis chaque paire est replacée dans le nouvel `index` avec son hash gardé, sans rien recalculer.

Un détail qui fait plaisir : `newMap` n'alloue que la petite structure, avec `entries` et `index` à `NULL` et `capacity` à 0. Le premier `map_set` trouve `size == capacity` (0 = 0), et c'est `map_rebuild` qui crée les deux tableaux. Un map vide ne coûte presque rien, et il n'y a pas de cas « première insertion » à écrire.

> [!note]
> Ce dessin en deux tableaux est celui des `dict` de Python depuis la version 3.6 (2016), proposé par Raymond Hettinger. Avant, un `dict` Python rendait ses clés dans le désordre. Java fait autrement avec `LinkedHashMap` : une table classique, plus une liste doublement chaînée qui relie les paires dans l'ordre d'arrivée.

```c fichier="src/map.c" lignes="184-200" focus="193-194"
void map_set(map_t *map, value_t *key, value_t *value){
    uint32_t hash = map_hash_key(key);
    int position = map_find(map,key,hash);

    if(position >= 0){
        map->entries[position].value = value;
        return;
    }

    if(map->size == map->capacity)
        map_rebuild(map);

    map->entries[map->size] = (map_entry_t){.key = key, .value = value, .hash = hash};
    map_index_add(map,hash,map->size);
    map->size += 1;
    map->count += 1;
}
```

Reste la question des clés. Une clé est une chaîne, un nombre ou un élément d'énumération, et chaque sorte a son hash :

```c fichier="src/map.c" lignes="58-70"
// Une chaîne ne change jamais : son hash se calcule une seule fois.
// Un élément d'énumération n'existe qu'une fois : son adresse suffit.
static uint32_t map_hash_key(value_t *key){
    if(key->type == NUMBER)
        return map_hash_number(AS_NUMBER(key));
    if(key->type == ENUM_VALUE)
        return map_hash_bits((uint64_t)(uintptr_t)key);

    string_t *string = AS_STRING(key);
    if(string->hash == 0)
        string->hash = map_hash_text(string->str,string->lenght);
    return string->hash;
}
```

Tu retrouves le hash gardé dans la chaîne, qu'on a promis en 5.5. `map_hash_text` (lignes 37 à 44) est le hash FNV-1a, octet par octet, mélangé ensuite par `map_mix`. Il ne rend jamais 0, pour que 0 puisse vouloir dire « pas encore calculé ». Le hash d'un nombre lit les bits du `double`, après avoir ajouté `0.0` pour que `0` et `-0`, qui ont des bits différents, donnent la même clé.

Et un caractère ? `t[mot[i]]` est trop pratique pour l'interdire. `Value_map_key`, juste avant chaque lecture ou écriture, le change en chaîne d'un octet, et refuse le reste :

```c fichier="src/Value.c" lignes="141-148"
// Une clé est une chaîne, un nombre ou un élément d'énumération. Un caractère compte comme une chaîne d'un seul octet.
static value_t *Value_map_key(ast_t *node, value_t *key){
    if(key->type == STRING || key->type == NUMBER || key->type == ENUM_VALUE)
        return key;
    if(key->type == CHAR)
        return makeStringLenght(&AS_CHAR(key),1);
    visitor_error(node,"une clé de map doit être str, number ou un élément d'énumération, pas %s",Value_type_name(key));
}
```

Une liste ne peut pas être une clé. Elle peut changer après avoir été rangée, et son hash deviendrait faux, pour la même raison qu'en 5.5.

```ceflo fichier="cles.cf"
var t = {}
t[1] = "un nombre"
t["1"] = "une chaîne"
t['a'] = "un caractère"
println(t)
println(t["a"])
t[[1]] = "une liste"
```

```terminal
$ ./Ceflo_ref cles.cf
{ 1 : "un nombre" , "1" : "une chaîne" , "a" : "un caractère" }
un caractère
erreur: une clé de map doit être str, number ou un élément d'énumération, pas list
  --> cles.cf:7:2
    7 | t[[1]] = "une liste"
      |  ^
```

`1` et `"1"` sont deux clés : `map_find` cherche un nombre parmi les nombres et un texte parmi les textes. Le `'a'` rangé est devenu la chaîne `"a"`, et c'est sous ce nom qu'on le relit.

## 5.7 Les méthodes, une table de noms

`l.append(4)`, `t.keys()`, `"a,b".split(",")` : les valeurs ont des méthodes. Le visitor du chapitre 7 calculera `l`, les arguments, puis demandera à `Value.c` d'appeler la méthode qui porte ce nom. Chaque type a son tableau de paires nom et fonction :

```c fichier="src/Value.c" lignes="726-735"
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
```

`length` a été ajouté à côté de `lenght` pour ceux qui l'écrivent correctement : les deux noms désignent la même fonction. La recherche est un simple parcours, qui rend `NULL` si le nom n'existe pas :

```c fichier="src/Value.c" lignes="762-782"
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
```

`NULL` veut dire « pas de méthode de ce nom », et c'est l'appelant qui écrira le message d'erreur, parce que lui seul sait où pointer. Un `strcmp` par méthode à chaque appel, c'est lent, mais avec sept méthodes au plus, la différence ne se voit pas encore.

Une méthode reçoit `self`, la valeur sur laquelle on l'appelle, et vérifie elle-même ses arguments. Voici `lenght`, la plus courte :

```c fichier="src/Value.c" lignes="522-526"
static value_t *value_List_lenght(struct VISITOR_STRUCT *visitor, ast_t *node, value_t *self, value_t **args, int argc){
    (void)visitor;(void)args;
    Value_check_args(node,"lenght",argc,0);
    return makeNumber(AS_LIST(self)->Size);
}
```

`Value_check_args` (ligne 496) arrête le programme avec « `lenght` prend 0 argument(s), mais en reçoit 1 » si on lui passe un argument. Les `(void)` disent au compilateur que les paramètres inutilisés le sont exprès, pour que `-Wextra` ne s'en plaigne pas.

```ceflo fichier="methodes.cf"
var mots = "un,deux,trois".split(",")
mots.append("quatre")
println(mots, " ", mots.lenght())
println("{} a {} ans".format("Yann", 22))
var t = { a: 1 }
println(t.get("b", 0), " ", t.has("a"), " ", t.keys())
mots.push("cinq")
```

```terminal
$ ./Ceflo_ref methodes.cf
[ "un" , "deux" , "trois" , "quatre" ] 4
Yann a 22 ans
0 true [ "a" ]
erreur: list n'a pas de méthode `push`
  --> methodes.cf:7:6
    7 | mots.push("cinq")
      |      ^
```

## 5.8 Les opérateurs

Dernière pièce : calculer `left op right`. Tout tient dans `src/value_expr.c`, 132 lignes, dont voici la porte d'entrée :

```c fichier="src/value_expr.c" lignes="103-132"
value_t *value_expr(visitor_t *visitor, ast_t *node, value_t *left, TokenEnum op, value_t *right){
    switch(op){
        case TOKEN_EQUAL_EQUAL:
            return makeBool(Value_equals(left,right));
        case TOKEN_DIF:
            return makeBool(!Value_equals(left,right));
        case TOKEN_INF:
        case TOKEN_INF_EQUAL:
        case TOKEN_SUP:
        case TOKEN_SUP_EQUAL:
            return value_compare(node,left,op,right);
        case TOKEN_PLUS:
            if(left->type == STRING || right->type == STRING)
                return value_concat(visitor,left,right);
            if(left->type == LIST && right->type == LIST)
                return value_list_concat(left,right);
            break;
        case TOKEN_MULT:
            if(left->type == STRING && right->type == NUMBER)
                return value_repeat(node,left,AS_NUMBER(right));
            break;
        default:
            break;
    }

    if(!is_numeric(left) || !is_numeric(right))
        value_expr_error(node,left,op,right);

    return value_arithmetic(node,Value_as_double(left),op,Value_as_double(right));
}
```

L'opérateur est le `TokenEnum` du lexer, gardé tel quel dans le noeud : pas besoin d'inventer une autre énumération. Lis la fonction de haut en bas comme une suite de questions. Est-ce une égalité ? Une comparaison ? Un `+` avec une chaîne d'un côté, ou deux listes ? Un `*` entre une chaîne et un nombre ? Si aucun cas particulier ne répond, il reste le calcul, qui n'accepte que des `number`, des `bool` et des `char`.

`value_concat` (lignes 24 à 32) est la raison pour laquelle `1 + "a"` donne `"1a"` : il écrit le texte des deux côtés avec `Value_to_buffer`, dans le même tampon. Ce qui veut dire que `"liste : " + [1, 2]` marche aussi, et donne `liste : [ 1 , 2 ]`.

Le calcul lui-même est un `switch` sur des `double` :

```c fichier="src/value_expr.c" lignes="61-77"
static value_t *value_arithmetic(ast_t *node, double left, TokenEnum op, double right){
    switch(op){
        case TOKEN_PLUS: return makeNumber(left + right);
        case TOKEN_MINUS: return makeNumber(left - right);
        case TOKEN_MULT: return makeNumber(left * right);
        case TOKEN_DIV:
            if(right == 0)
                visitor_error(node,"division par zéro");
            return makeNumber(left / right);
        case TOKEN_MODULO:
            if(right == 0)
                visitor_error(node,"modulo par zéro");
            return makeNumber(fmod(left,right));
        default:
            return NULL;
    }
}
```

En C, `1.0 / 0` donne l'infini, sans erreur. CeFlo refuse : un script qui divise par zéro a presque toujours un bug, et le dire tout de suite, avec la ligne, vaut mieux qu'un `inf` qui se promène. `%` est `fmod`, le modulo des `double`, qui garde le signe de la gauche : `-7 % 3` vaut `-1`, comme en C.

Il reste le cas où rien ne va. Une seule fonction écrit le message, pour qu'il ait toujours la même forme :

```c fichier="src/value_expr.c" lignes="13-21"
static _Noreturn void value_expr_error(ast_t *node, value_t *left, TokenEnum op, value_t *right){
    visitor_error(
        node,
        "l'opérateur `%s` ne s'applique pas à %s et %s",
        token_to_string(op),
        Value_type_name(left),
        Value_type_name(right)
    );
}
```

`_Noreturn` dit au compilateur que la fonction ne revient jamais : `visitor_error` affiche le message et appelle `exit(1)`. C'est ce qui permet à `value_expr` de continuer après l'appel comme si de rien n'était, sans `return` ni avertissement. `node` est le noeud `AST_BINOP`, dont la position est celle de l'opérateur (le parser l'a créé sur ce jeton) : la flèche tombe pile dessous.

On peut appeler `value_expr` sans aucun arbre, depuis un essai C. `node` vaut alors `NULL`, et l'erreur s'affiche sans position :

```c fichier="livre/essais/chapitres-05-08/ch5/essai_operateurs.c"
static void montre(value_t *left, TokenEnum op, value_t *right){
    value_t *result = value_expr(NULL, NULL, left, op, right);
    Value_print(NULL, stdout, result);
    printf("\n");
}

int main(void){
    ceflo_stack_init();
    montre(makeNumber(1), TOKEN_PLUS, makeString("a"));
    montre(makeChar('A'), TOKEN_PLUS, makeNumber(1));
    montre(makeString("ab"), TOKEN_MULT, makeNumber(3));
    montre(makeString("abc"), TOKEN_INF, makeString("abd"));
    montre(makeList(), TOKEN_MINUS, makeNumber(1));
    printf("jamais affiché\n");
    return 0;
}
```

```terminal
$ ./compile.sh essai_operateurs
$ ./essai_operateurs; echo "code : $?"
1a
66
ababab
true
erreur: l'opérateur `-` ne s'applique pas à list et number
code : 1
```

Les cinq premières lignes de `surprises.cf`, au début du chapitre, sont là, calculées sans lexer, sans parser et sans visitor. Et `jamais affiché` ne s'affiche jamais : `visitor_error` a terminé le programme avec le code 1.

```terminal
$ cat op.cf
var x = [1] - 1
$ ./Ceflo_ref op.cf
erreur: l'opérateur `-` ne s'applique pas à list et number
  --> op.cf:1:13
    1 | var x = [1] - 1
      |             ^
```

```terminal
$ cat op.cf
println("a" < 3)
$ ./Ceflo_ref op.cf
erreur: l'opérateur `<` ne s'applique pas à str et number
  --> op.cf:1:13
    1 | println("a" < 3)
      |             ^
```

Et quelques cas qui marchent, parce qu'un cas particulier les a acceptés :

```terminal
$ cat op.cf
println([1] + [2, 3])
$ ./Ceflo_ref op.cf
[ 1 , 2 , 3 ]
$ cat op.cf
println(true + true)
$ ./Ceflo_ref op.cf
2
$ cat op.cf
println("ab" * 2.7, "|", "ab" * -1, "|")
$ ./Ceflo_ref op.cf
abab||
```

`"ab" * 2.7` répète deux fois : `value_repeat` (lignes 44 à 59) tronque le nombre, et un nombre négatif donne la chaîne vide. Il refuse aussi de fabriquer plus d'un gigaoctet, pour qu'un `"x" * 2000000000` donne « chaîne trop longue » au lieu d'avaler toute la mémoire.

Tu as remarqué ce qui manque : `and` et `or`. Ils ne sont pas dans `value_expr`, et ils ne peuvent pas y être. Quand `value_expr` est appelée, les deux côtés ont déjà été calculés. Or `x != none and x.nom == "Yann"` ne doit pas calculer la droite si la gauche est fausse. Seul celui qui parcourt l'arbre peut décider de ne pas visiter un noeud : ce sera le visitor.

## Défis

1. **Des entiers partagés.** Fais rendre à `makeNumber` une valeur unique pour les entiers de 0 à 255, comme le fait CPython pour ses petits entiers. Il faut un tableau `static`, un test, et un `gc_root` pour chaque valeur (regarde `Value_shared`). Puis mesure `bench/boucle.cf` avant et après avec `time`. Le gain est-il à la hauteur de ce que tu attendais ? Le chapitre 8 t'aidera à comprendre le résultat.

2. **L'égalité des maps.** Rends `{ a: 1 } == { a: 1 }` vrai. Ajoute un `case MAP` dans `Value_equals` : compare `count`, puis cherche chaque clé de gauche dans le map de droite avec `map_get`, en sautant les trous (`key == NULL`). Que doit donner `t == t` quand `t` se contient lui-même ? Teste-le.

3. **Une méthode de chaîne.** Ajoute `"abc".upper()`, qui rend `"ABC"`. Écris `value_Str_upper` sur le modèle de `value_Str_lenght` (ligne 656), fabrique la chaîne avec `makeStringLenght` puis change ses octets avec `toupper`, et ajoute la paire dans `attr_value_str` (ligne 754). Pourquoi a-t-on le droit de modifier ce texte, alors qu'une chaîne ne change jamais ?

4. **L'opérateur puissance.** Ajoute `**`, avec `2 ** 10` qui vaut 1024. Il faut un jeton dans `src/include/token.h`, une règle dans le parser (chapitre 3 : à quel étage le mettre, et doit-il être associatif à droite comme en Python, où `2 ** 3 ** 2` vaut 512 ?), et un `case` dans `value_arithmetic` qui appelle `pow`. Fais aussi marcher `3 * "ab"` comme `"ab" * 3`, dans le `case TOKEN_MULT` de `value_expr`.

## Note de conception

Toute la personnalité d'un langage de script tient dans ses règles de conversion, et elles tiennent toutes dans `value_expr`. JavaScript a choisi de tout convertir : `"5" - 1` vaut 4, `"5" + 1` vaut `"51"`, et `[] + {}` vaut `"[object Object]"`. Chaque règle a l'air raisonnable seule, et ensemble elles donnent des présentations célèbres où la salle rit à chaque diapositive. Python a choisi de presque rien convertir : `"5" + 1` est une erreur, et il faut écrire `str(1)` ou `int("5")`.

CeFlo est entre les deux, et j'assume ce choix. Une seule conversion se fait toute seule, celle du texte : dès qu'une chaîne est d'un côté du `+`, l'autre côté devient du texte. C'est la règle de Java, et elle sert à une seule chose, que tout le monde fait cent fois par jour : écrire `"x = " + x`. Tout le reste est une erreur avec la ligne et la flèche. `"5" - 1` ne vaut rien du tout, il arrête le programme.

Le cas de `bool` et `char` dans les calculs est plus discutable. `'A' + 1` qui vaut 66 vient du C, et sert à manipuler des caractères : `@char('A' + 1)` donne `'B'` (`GRAMMAIRE.md`), et le lexer écrit en CeFlo de `exemple/6_lexer.cf` teste un chiffre avec `current() >= '0' and current() <= '9'`, qui passe par la même règle. `true + true` qui vaut 2 n'a pas de bonne raison d'exister, sinon qu'interdire l'un sans l'autre aurait compliqué `is_numeric`. Si c'était à refaire, je l'interdirais.

La leçon à garder pour ton propre langage : écris ces règles dans un seul fichier, comme `value_expr.c`, et écris leurs messages d'erreur en même temps qu'elles. Une règle de conversion qu'on ne sait pas expliquer en une phrase dans un message d'erreur est une règle qui surprendra quelqu'un.

On a maintenant tout ce qu'un programme manipule : des nombres, du texte, des listes, des maps, et les règles pour les combiner. Il manque une chose, et sans elle aucun vrai programme ne tourne. Dans `var x = 2` puis `x + 1`, qui se souvient que `x` vaut 2 ? Et quand une fonction a sa propre variable `x`, laquelle des deux `x + 1` doit-il lire ? C'est le travail des portées.
